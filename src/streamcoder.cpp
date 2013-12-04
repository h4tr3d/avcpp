#include <cassert>
#include <iostream>

#include "audiosamples.h"
#include "streamcoder.h"
#include "avutils.h"

using namespace std;

namespace av
{


StreamCoder::StreamCoder()
{
    init();
}

StreamCoder::~StreamCoder()
{
    close();
}

StreamCoder::StreamCoder(const StreamPtr &stream)
{
    init();

    this->stream = stream;
    context = stream->getAVStream()->codec;

    CodecPtr codec;
    if (stream->getDirection() == DECODING)
    {
        codec = Codec::findDecodingCodec(context->codec_id);
    }
    else
    {
        codec = Codec::findEncodingCodec(context->codec_id);
    }

    if (codec)
        setCodec(codec);

    direction = stream->getDirection();
    setTimeBase(context->time_base);
}


void StreamCoder::setCodec(const CodecPtr &codec)
{
    if (!codec)
    {
        cout << "Cannot set codec to null codec\n";
        return;
    }

    AVCodec *avCodec = codec->getAVCodec();

    if (!context)
    {
        cout << "Codec context does not allocated\n";
        return;
    }

    if (context->codec_id != CODEC_ID_NONE && context->codec)
    {
        cout << "Codec already set to: " << context->codec_id << " / " << context->codec_name << ", ignoring." << endl;
        return;
    }

    if (this->codec == codec)
    {
        cout << "Try set same codec\n";
        return;
    }

    context->codec_id   = avCodec ? avCodec->id : CODEC_ID_NONE;
    context->codec_type = avCodec ? avCodec->type : AVMEDIA_TYPE_UNKNOWN;
    context->codec      = avCodec;

    if (avCodec->pix_fmts != 0)
    {
        context->pix_fmt = *(avCodec->pix_fmts); // assign default value
    }

    if (avCodec->sample_fmts != 0)
    {
        context->sample_fmt = *(avCodec->sample_fmts);
    }

//    else
//    {
//        context->pix_fmt = PIX_FMT_NONE;
//    }

    this->codec = codec;
}


void StreamCoder::init()
{
    isOpenedFlag          = false;
    fakePtsTimeBase       = Rational(1, AV_TIME_BASE);
    context               = 0;
    fakeCurrPts           = AV_NOPTS_VALUE;
    fakeNextPts           = AV_NOPTS_VALUE;
    defaultAudioFrameSize = 576;
}

ssize_t StreamCoder::decodeCommon(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    assert(context);

    std::shared_ptr<AVFrame> frame(avcodec_alloc_frame(), AvDeleter());
    if (!frame)
    {
        return -1;
    }

    if (!decodeProc)
    {
        return -1;
    }

    if (offset >= inPacket->getSize())
    {
        return -1;
    }

    outFrame->setComplete(false);

    int frameFinished = 0;

    std::shared_ptr<AVPacket> pkt((AVPacket*)av_malloc(sizeof(AVPacket)), AvDeleter());
    av_init_packet(pkt.get());
    *pkt = *inPacket->getAVPacket();
    pkt->data     = const_cast<uint8_t*>(inPacket->getData());
    pkt->size     = inPacket->getSize();
    pkt->destruct = 0; // prevent data free

    pkt->data += offset;
    pkt->size -= offset;

    context->reordered_opaque = inPacket->getPts();
    ssize_t totalDecode = 0;
    int     iterations = 0;
    do
    {
        ++iterations;
        int decoded = decodeProc(context, frame.get(), &frameFinished, pkt.get());
        if (decoded < 0)
        {
            return totalDecode;
        }

        totalDecode += decoded;
        pkt->data   += decoded;
        pkt->size   -= decoded;
    }
    while (frameFinished == 0 && pkt->size > 0);

    if (frameFinished)
    {
        *outFrame.get() = frame.get();

        outFrame->setTimeBase(getTimeBase());

        int64_t packetTs = frame->reordered_opaque;
        if (packetTs == AV_NOPTS_VALUE)
        {
            packetTs = inPacket->getDts();
        }

        if (packetTs != AV_NOPTS_VALUE)
        {
            //int64_t nextPts = fakePtsTimeBase.rescale(packetTs, outFrame->getTimeBase());
            int64_t nextPts = packetTs;

            if (nextPts < fakeNextPts && inPacket->getPts() != AV_NOPTS_VALUE)
            {
                nextPts = inPacket->getPts();
            }

            fakeNextPts = nextPts;
        }

        fakeCurrPts = fakeNextPts;
        //double frameDelay = inPacket->getTimeBase().getNumerator();
        double frameDelay = inPacket->getTimeBase().getDouble();
        frameDelay += outFrame->getAVFrame()->repeat_pict * (frameDelay * 0.5);

        fakeNextPts += (int64_t) frameDelay;

        outFrame->setStreamIndex(inPacket->getStreamIndex());
        if (fakeCurrPts != AV_NOPTS_VALUE)
            outFrame->setPts(inPacket->getTimeBase().rescale(fakeCurrPts, outFrame->getTimeBase()));
        outFrame->setComplete(true);
    }

    return totalDecode;
}

ssize_t StreamCoder::encodeCommon(const PacketPtr &outPacket, const FramePtr &inFrame,
                              int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *),
                              const EncodedPacketHandler &onPacketHandler)
{
    if (!outPacket)
    {
        cerr << "Null packet can't be used as target for encoded data\n";
        return -1;
    }

    if (!inFrame)
    {
        cerr << "Null frame can't be encoded\n";
        return -1;
    }

    if (!isValidForEncode())
    {
        return -1;
    }

    if (!encodeProc)
    {
        return -1;
    }

    // Be thread-safe, make our own copy of encoded frame
    FramePtr frame = inFrame->clone();

    // Change frame's pts and time base to coder's based values
    frame->setTimeBase(getTimeBase());

    if (context->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        PacketPtr workPacket = outPacket;
        if (!workPacket)
        {
            workPacket = PacketPtr(new Packet());
        }

        // set timebase to coder timebase
        workPacket->setTimeBase(getTimeBase());

        AudioSamplesPtr samples = std::dynamic_pointer_cast<AudioSamples>(frame);

        int gotPacket;
        int stat = encodeProc(context, workPacket->getAVPacket(), samples->getAVFrame(), &gotPacket);

        if (stat == 0)
        {
            if (gotPacket)
            {
                // Change timebase to target stream timebase, it also recalculate all TS values
                // (pts, dts, duration)
                workPacket->setTimeBase(stream->getTimeBase());

                workPacket->setStreamIndex(samples->getStreamIndex());

                if (samples->getFakePts() != AV_NOPTS_VALUE)
                {
                    workPacket->setFakePts(samples->getTimeBase()
                                           .rescale(samples->getFakePts(),
                                                    workPacket->getTimeBase()));
                }

                if (onPacketHandler)
                {
                    onPacketHandler(workPacket);
                }
            }
        }
        else
        {
            cout << "Encode error:             " << stat << endl;
            cout << "coded_frame PTS:          " << context->coded_frame->pts << endl;
            cout << "input_frame PTS:          " << inFrame->getPts() << endl;
            return stat;
        }
    }
    else if (context->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        PacketPtr workPacket = outPacket;
        if (!workPacket)
        {
            workPacket = PacketPtr(new Packet());
        }

        int gotPacket;
        int stat = encodeProc(context, workPacket->getAVPacket(), frame->getAVFrame(), &gotPacket);

        if (stat == 0)
        {
            if (gotPacket)
            {
                // Packet always must be in AVStream time base units
                workPacket->setTimeBase(stream->getTimeBase());

                if (context->coded_frame->pts != AV_NOPTS_VALUE)
                {
                    workPacket->setPts(getTimeBase().rescale(context->coded_frame->pts,
                                                             workPacket->getTimeBase()));
                }

                if (workPacket->getDts() == AV_NOPTS_VALUE && workPacket->getPts() != AV_NOPTS_VALUE)
                {
                    workPacket->setDts(workPacket->getPts());
                }
                else
                {
                    workPacket->setDts(getTimeBase().rescale(workPacket->getDts(),
                                                             workPacket->getTimeBase()));
                }


                workPacket->setKeyPacket(!!context->coded_frame->key_frame);
                workPacket->setStreamIndex(frame->getStreamIndex());

                if (onPacketHandler)
                {
                    onPacketHandler(workPacket);
                }
            }
        }
        else
        {
            cout << "Encode error:             " << stat << endl;
            cout << "coded_frame PTS:          " << context->coded_frame->pts << endl;
            cout << "input_frame PTS:          " << inFrame->getPts() << endl;
            return stat;
        }
    }

    return 0;
}


bool StreamCoder::open()
{
    if (context && codec)
    {
        // HACK: set threads to 1
        AVDictionary *opts = 0;
        av_dict_set(&opts, "threads", "1", 0);
        ////////////////////////////////////////////////////////////////////////////////////////////

        bool ret = true;
        int stat = avcodec_open2(context, codec->getAVCodec(), &opts);
        if (stat < 0)
        {
            ret = false;
        }
        else
        {
            isOpenedFlag = true;
        }

        av_dict_free(&opts);

        return ret;
    }

    return false;
}

bool StreamCoder::close()
{
    if (context && codec && isOpenedFlag)
    {
        avcodec_close(context);
        isOpenedFlag = false;
        return true;
    }

    return false;
}

Rational StreamCoder::getTimeBase()
{
    return (context ? context->time_base : Rational());
}

void StreamCoder::setTimeBase(const Rational &value)
{
    if (context)
    {
        context->time_base = value.getValue();
    }
}

StreamPtr StreamCoder::getStream() const
{
    return stream;
}

AVMediaType StreamCoder::getCodecType() const
{
    return (context ? context->codec_type : AVMEDIA_TYPE_UNKNOWN);
}

int StreamCoder::getWidth() const
{
    return (context ? context->width : -1);
}

int StreamCoder::getHeight() const
{
    return (context ? context->height : -1);
}

PixelFormat StreamCoder::getPixelFormat() const
{
    return (context ? context->pix_fmt : PIX_FMT_NONE);
}

Rational StreamCoder::getFrameRate()
{
    if (!isOpenedFlag && stream)
    {
        return stream->getFrameRate();
    }

    return Rational();
}

int32_t StreamCoder::getBitRate() const
{
    return (context ? context->bit_rate : 0);
}

pair<int,int> StreamCoder::getBitRateRange() const
{
    return (context ? std::pair<int,int>(context->rc_min_rate, context->rc_max_rate) : std::pair<int,int>());
}

int32_t StreamCoder::getGlobalQuality()
{
    return (context ? context->global_quality : FF_LAMBDA_MAX);
}

int32_t StreamCoder::getGopSize()
{
    return (context ? context->gop_size : -1);
}

int StreamCoder::getBitRateTolerance() const
{
    return (context ? context->bit_rate_tolerance : -1);
}

int StreamCoder::getStrict() const
{
    return (context ? context->strict_std_compliance : 0);
}

int StreamCoder::getMaxBFrames() const
{
    return (context ? context->max_b_frames : 0);
}

int StreamCoder::getFrameSize() const
{
    assert(context);

    return context->frame_size;
}

void StreamCoder::setWidth(int w)
{
    if (context)
    {
        context->width = w;
    }
}

void StreamCoder::setHeight(int h)
{
    if (context)
        context->height = h;
}

void StreamCoder::setPixelFormat(PixelFormat pixelFormat)
{
    if (context)
        context->pix_fmt = pixelFormat;
}

void StreamCoder::setFrameRate(const Rational &frameRate)
{
    if (!isOpenedFlag && stream)
        stream->setFrameRate(frameRate);
}

void StreamCoder::setBitRate(int32_t bitRate)
{
    if (context && !isOpenedFlag)
        context->bit_rate = bitRate;
}

void StreamCoder::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    if (context && !isOpenedFlag)
    {
        context->rc_min_rate = bitRateRange.first;
        context->rc_max_rate = bitRateRange.second;
    }
}

void StreamCoder::setGlobalQuality(int32_t quality)
{
    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    if (context)
        context->global_quality = quality;
}

void StreamCoder::setGopSize(int32_t size)
{
    if (context)
        context->gop_size = size;
}

void StreamCoder::setBitRateTolerance(int bitRateTolerance)
{
    if (context)
        context->bit_rate_tolerance = bitRateTolerance;
}

void StreamCoder::setStrict(int strict)
{
    if (context)
    {
        if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        {
            strict = FF_COMPLIANCE_EXPERIMENTAL;
        }
        else if (strict > FF_COMPLIANCE_VERY_STRICT)
        {
            strict = FF_COMPLIANCE_VERY_STRICT;
        }

        context->strict_std_compliance = strict;
    }
}

void StreamCoder::setMaxBFrames(int maxBFrames)
{
    if (context)
        context->max_b_frames = maxBFrames;
}

int StreamCoder::getSampleRate() const
{
    return (context ? context->sample_rate : -1);
}

int StreamCoder::getChannels() const
{
    if (!context)
    {
        return 0;
    }

    if (context->channels)
    {
        return context->channels;
    }

    if (context->channel_layout)
    {
        return av_get_channel_layout_nb_channels(context->channel_layout);
    }

    return 0;
}

AVSampleFormat StreamCoder::getSampleFormat() const
{
    return (context ? context->sample_fmt : AV_SAMPLE_FMT_NONE);
}

uint64_t StreamCoder::getChannelLayout() const
{
    if (!context)
    {
        return 0;
    }

    if (context->channel_layout)
    {
        return context->channel_layout;
    }

    if (context->channels)
    {
        return av_get_default_channel_layout(context->channels);
    }

    return 0;
}

int StreamCoder::getAudioFrameSize() const
{
    int result = 0;
    if (codec && codec->getAVCodec()->type == AVMEDIA_TYPE_AUDIO)
    {
        if (context)
        {
            if (context->frame_size <= 1)
            {
                // Rats; some PCM encoders give a frame size of 1, which is too
                //small.  We pick a more sensible value.
                result = defaultAudioFrameSize;
            }
            else
            {
                result = context->frame_size;
            }
        }
    }

    return result;
}

int StreamCoder::getDefaultAudioFrameSize() const
{
    return defaultAudioFrameSize;
}

void StreamCoder::setSampleRate(int sampleRate)
{
    assert(context);

    if (!isOpenedFlag)
    {
        int sr = guessValue(sampleRate, context->codec->supported_samplerates, EqualComparator<int>(0));
        clog << "========= Input sampleRate: " << sampleRate << ", guessed sample rate: " << sr << endl;

        if (sr > 0)
            context->sample_rate = sr;
    }
}

void StreamCoder::setChannels(int channels)
{
    if (context && !isOpenedFlag && channels > 0)
    {
        context->channels = channels;

        // Make channels and channel_layout sync
        if (context->channel_layout == 0 ||
            av_get_channel_layout_nb_channels(context->channel_layout) != channels)
        {
            context->channel_layout = av_get_default_channel_layout(channels);
        }
    }
}

void StreamCoder::setSampleFormat(AVSampleFormat sampleFormat)
{
    if (context && !isOpenedFlag && sampleFormat >= 0)
    {
        context->sample_fmt = sampleFormat;
    }
}

void StreamCoder::setChannelLayout(uint64_t layout)
{
    if (context && !isOpenedFlag && layout > 0)
    {
        context->channel_layout = layout;

        // Make channels and channel_layout sync
        if (context->channels == 0 ||
            av_get_default_channel_layout(context->channels) != layout)
        {
            context->channels = av_get_channel_layout_nb_channels(layout);
        }
    }
}

void StreamCoder::setAudioFrameSize(int frameSize)
{
    if (context)
        context->frame_size = frameSize;
}

void StreamCoder::setDefaultAudioFrameSize(int frameSize)
{
    defaultAudioFrameSize = frameSize;
}

void StreamCoder::setFlags(int32_t flags)
{
    if (context)
        context->flags = flags;
}

void StreamCoder::addFlags(int32_t flags)
{
    if (context)
        context->flags |= flags;
}

void StreamCoder::clearFlags(int32_t flags)
{
    if (context)
        context->flags &= ~flags;
}


int32_t StreamCoder::getFlags()
{
    return (context ? context->flags : 0);
}


ssize_t StreamCoder::decodeVideo(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset)
{
    return decodeCommon(outFrame, inPacket, offset, avcodec_decode_video2);
}


ssize_t StreamCoder::encodeVideo(const PacketPtr &outPacket, const VideoFramePtr &inFrame, const EncodedPacketHandler &onPacketHandler)
{
    if (inFrame->getPixelFormat() != getPixelFormat())
    {
        cerr << "Frame does not have same PixelFormat to coder: " << av_get_pix_fmt_name(inFrame->getPixelFormat()) << endl;
        return -1;
    }

    if (inFrame->getWidth() != getWidth())
    {
        cerr << "Frame does not have same Width to coder\n";
        return -1;
    }

    if (inFrame->getHeight() != getHeight())
    {
        cerr << "Frame does not have same Height to coder\n";
        return -1;
    }

    return encodeCommon(outPacket, inFrame, avcodec_encode_video2, onPacketHandler);
}

ssize_t StreamCoder::decodeAudio(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset)
{
    return decodeCommon(outFrame, inPacket, offset, avcodec_decode_audio4);
}

ssize_t StreamCoder::encodeAudio(const PacketPtr &outPacket, const FramePtr &inFrame, const EncodedPacketHandler &onPacketHandler)
{
    // TODO: additional checks like encodeVideo()
    return encodeCommon(outPacket, inFrame, avcodec_encode_audio2, onPacketHandler);
}

bool StreamCoder::isValidForEncode()
{
    if (!isOpenedFlag)
    {
        cerr << "You must open coder before encoding\n";
        return false;
    }

    if (!context)
    {
        cerr << "Codec context does not exists\n";
        return false;
    }

//    if (context->codec_type != AVMEDIA_TYPE_VIDEO)
//    {
//        cerr << "Attempting to encode video with non video coder\n";
//        return false;
//    }

    if (direction == DECODING)
    {
        cerr << "Decoding coder does not valid for encoding\n";
        return false;
    }

    if (!codec)
    {
        cerr << "Codec does not set\n";
        return false;
    }

    if (!codec->canEncode())
    {
        cerr << "Codec can't be used for Encode\n";
        return false;
    }

    fakeCurrPts = AV_NOPTS_VALUE;
    fakeNextPts = AV_NOPTS_VALUE;

    return true;
}


} // ::av
