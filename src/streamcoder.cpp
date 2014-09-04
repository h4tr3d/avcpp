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

    this->m_stream = stream;
    m_context = stream->getAVStream()->codec;

    CodecPtr codec;
    if (stream->getDirection() == DECODING)
    {
        codec = Codec::findDecodingCodec(m_context->codec_id);
    }
    else
    {
        codec = Codec::findEncodingCodec(m_context->codec_id);
    }

    if (codec)
        setCodec(codec);

    m_direction = stream->getDirection();
}


void StreamCoder::setCodec(const CodecPtr &codec)
{
    if (!codec)
    {
        cout << "Cannot set codec to null codec\n";
        return;
    }

    const AVCodec *avCodec = codec->getAVCodec();

    if (!m_context)
    {
        cout << "Codec context does not allocated\n";
        return;
    }

    if (m_codec && (m_codec == codec || m_codec->getAVCodec() == avCodec))
    {
        cout << "Try set same codec\n";
        return;
    }

    m_context->codec_id   = avCodec ? avCodec->id : CODEC_ID_NONE;
    m_context->codec_type = avCodec ? avCodec->type : AVMEDIA_TYPE_UNKNOWN;
    m_context->codec      = avCodec;

    if (avCodec->pix_fmts != 0)
    {
        m_context->pix_fmt = *(avCodec->pix_fmts); // assign default value
    }

    if (avCodec->sample_fmts != 0)
    {
        m_context->sample_fmt = *(avCodec->sample_fmts);
    }

//    else
//    {
//        context->pix_fmt = PIX_FMT_NONE;
//    }

    this->m_codec = codec;
}


void StreamCoder::init()
{
    m_openedFlag            = false;
    m_fakePtsTimeBase       = Rational(1, AV_TIME_BASE);
    m_context               = nullptr;
    m_fakeCurrPts           = AV_NOPTS_VALUE;
    m_fakeNextPts           = AV_NOPTS_VALUE;
    m_defaultAudioFrameSize = 576;
}

ssize_t StreamCoder::decodeCommon(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    assert(m_context);

    std::shared_ptr<AVFrame> frame(av_frame_alloc(), AvDeleter());
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

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt      = *inPacket->getAVPacket();
    pkt.data = inPacket->getData();
    pkt.size = inPacket->getSize();

    pkt.data += offset;
    pkt.size -= offset;

    m_context->reordered_opaque = inPacket->getPts();
    ssize_t totalDecode = 0;
    int     iterations = 0;
    do
    {
        ++iterations;
        int decoded = decodeProc(m_context, frame.get(), &frameFinished, &pkt);
        if (decoded < 0)
        {
            return totalDecode;
        }

        totalDecode += decoded;
        pkt.data   += decoded;
        pkt.size   -= decoded;
    }
    while (frameFinished == 0 && pkt.size > 0);

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

            if (nextPts < m_fakeNextPts && inPacket->getPts() != AV_NOPTS_VALUE)
            {
                nextPts = inPacket->getPts();
            }

            m_fakeNextPts = nextPts;
        }

        m_fakeCurrPts = m_fakeNextPts;
        //double frameDelay = inPacket->getTimeBase().getNumerator();
        double frameDelay = inPacket->getTimeBase().getDouble();
        frameDelay += outFrame->getAVFrame()->repeat_pict * (frameDelay * 0.5);

        m_fakeNextPts += (int64_t) frameDelay;

        outFrame->setStreamIndex(inPacket->getStreamIndex());
        if (m_fakeCurrPts != AV_NOPTS_VALUE)
            outFrame->setPts(inPacket->getTimeBase().rescale(m_fakeCurrPts, outFrame->getTimeBase()));
        outFrame->setComplete(true);
    }

    return totalDecode;
}

ssize_t StreamCoder::encodeCommon(const FramePtr &inFrame,
                                  int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *),
                                  const EncodedPacketHandler &onPacketHandler)
{
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

    PacketPtr workPacket = make_shared<Packet>();
    // set timebase to coder timebase
    workPacket->setTimeBase(getTimeBase());

    if (m_context->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        AudioSamplesPtr samples = std::dynamic_pointer_cast<AudioSamples>(frame);

        int gotPacket;
        int stat = encodeProc(m_context, workPacket->getAVPacket(), samples->getAVFrame(), &gotPacket);

        if (stat == 0)
        {
            if (gotPacket)
            {
                // Change timebase to target stream timebase, it also recalculate all TS values
                // (pts, dts, duration)
                workPacket->setTimeBase(m_stream->getTimeBase());

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
            cerr << "Encode error:             " << stat << endl;
            cerr << "coded_frame PTS:          " << m_context->coded_frame->pts << endl;
            cerr << "input_frame PTS:          " << inFrame->getPts() << endl;
            return stat;
        }
    }
    else if (m_context->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        AVPacket pkt;
        av_init_packet(&pkt);

        AVPacket *pktp = workPacket->getAVPacket();

        int gotPacket;
        int stat = encodeProc(m_context, pktp, frame->getAVFrame(), &gotPacket);

        if (stat == 0)
        {
            if (gotPacket)
            {
                // Packet always must be in AVStream time base units
                workPacket->setTimeBase(m_stream->getTimeBase());

                if (m_context->coded_frame->pts != AV_NOPTS_VALUE)
                {
                    workPacket->setPts(m_context->coded_frame->pts, getTimeBase());
                }

                if (workPacket->getDts() == AV_NOPTS_VALUE)
                {
                    workPacket->setDts(workPacket->getPts());
                }

                workPacket->setKeyPacket(!!m_context->coded_frame->key_frame);
                workPacket->setStreamIndex(frame->getStreamIndex());

                if (onPacketHandler)
                {
                    onPacketHandler(workPacket);
                }
            }
        }
        else
        {
            cerr << "Encode error:             " << stat << ", " << error2string(stat) << endl;
            cerr << "coded_frame PTS:          " << m_context->coded_frame->pts << endl;
            cerr << "input_frame PTS:          " << inFrame->getPts() << endl;
            return stat;
        }
    }

    return 0;
}


bool StreamCoder::open()
{
    if (m_context && m_codec)
    {
        // HACK: set threads to 1
        AVDictionary *opts = 0;
        av_dict_set(&opts, "threads", "1", 0);
        ////////////////////////////////////////////////////////////////////////////////////////////

        bool ret = true;
        int stat = avcodec_open2(m_context, m_codec->getAVCodec(), &opts);
        if (stat < 0)
        {
            ret = false;
        }
        else
        {
            m_openedFlag = true;
        }

        av_dict_free(&opts);

        return ret;
    }

    return false;
}

bool StreamCoder::close()
{
    if (m_context && m_codec && m_openedFlag)
    {
        avcodec_close(m_context);
        m_openedFlag = false;
        return true;
    }

    return false;
}

bool StreamCoder::copyContextFrom(const StreamPtr &other)
{
    if (!other || !m_context)
        return false;

    int stat = avcodec_copy_context(m_context, other->getAVStream()->codec);
    m_context->codec_tag = 0;

    return stat < 0 ? false : true;
}

bool StreamCoder::copyContextFrom(const StreamCoderPtr &other)
{
    if (!other || !m_context)
        return false;

    int stat = avcodec_copy_context(m_context, other->getAVCodecContext());
    m_context->codec_tag = 0;

    return stat < 0 ? false : true;
}

Rational StreamCoder::getTimeBase()
{
    return (m_context ? m_context->time_base : Rational());
}

void StreamCoder::setTimeBase(const Rational &value)
{
    if (m_context)
    {
        m_context->time_base = value.getValue();
    }
}

StreamPtr StreamCoder::getStream() const
{
    return m_stream;
}

AVMediaType StreamCoder::getCodecType() const
{
    return (m_context ? m_context->codec_type : AVMEDIA_TYPE_UNKNOWN);
}

void StreamCoder::setOption(const string& key, const string& val, int flags)
{
    if (m_context)
    {
        av_opt_set(m_context->priv_data, key.c_str(), val.c_str(), flags);
    }
}

int StreamCoder::getWidth() const
{
    return (m_context ? m_context->width : -1);
}

int StreamCoder::getCodedWidth() const
{
    return (m_context ? m_context->coded_width : -1);
}

int StreamCoder::getHeight() const
{
    return (m_context ? m_context->height : -1);
}

int StreamCoder::getCodedHeight() const
{
    return (m_context ? m_context->coded_height : -1);
}

PixelFormat StreamCoder::getPixelFormat() const
{
    return (m_context ? m_context->pix_fmt : PIX_FMT_NONE);
}

Rational StreamCoder::getFrameRate()
{
    if (!m_openedFlag && m_stream)
    {
        return m_stream->getFrameRate();
    }

    return Rational();
}

int32_t StreamCoder::getBitRate() const
{
    return (m_context ? m_context->bit_rate : 0);
}

pair<int,int> StreamCoder::getBitRateRange() const
{
    return (m_context ? std::pair<int,int>(m_context->rc_min_rate, m_context->rc_max_rate) : std::pair<int,int>());
}

int32_t StreamCoder::getGlobalQuality()
{
    return (m_context ? m_context->global_quality : FF_LAMBDA_MAX);
}

int32_t StreamCoder::getGopSize()
{
    return (m_context ? m_context->gop_size : -1);
}

int StreamCoder::getBitRateTolerance() const
{
    return (m_context ? m_context->bit_rate_tolerance : -1);
}

int StreamCoder::getStrict() const
{
    return (m_context ? m_context->strict_std_compliance : 0);
}

int StreamCoder::getMaxBFrames() const
{
    return (m_context ? m_context->max_b_frames : 0);
}

int StreamCoder::getFrameSize() const
{
    assert(m_context);

    return m_context->frame_size;
}

void StreamCoder::setWidth(int w)
{
    if (m_context)
    {
        m_context->width       = w;
        // We set width/height only on encoding, so, force set coded_width too
        m_context->coded_width = w;
    }
}

void StreamCoder::setHeight(int h)
{
    if (m_context)
    {
        m_context->height       = h;
        // We set width/height only on encoding, so, force set coded_height too
        m_context->coded_height = h;
    }
}

void StreamCoder::setCodedWidth(int w)
{
    if (m_context)
    {
        m_context->coded_width = w;
    }
}

void StreamCoder::setCodedHeight(int h)
{
    if (m_context)
    {
        m_context->coded_height = h;
    }
}


void StreamCoder::setPixelFormat(PixelFormat pixelFormat)
{
    if (m_context)
        m_context->pix_fmt = pixelFormat;
}

void StreamCoder::setFrameRate(const Rational &frameRate)
{
    if (!m_openedFlag && m_stream)
        m_stream->setFrameRate(frameRate);
}

void StreamCoder::setBitRate(int32_t bitRate)
{
    if (m_context && !m_openedFlag)
        m_context->bit_rate = bitRate;
}

void StreamCoder::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    if (m_context && !m_openedFlag)
    {
        m_context->rc_min_rate = bitRateRange.first;
        m_context->rc_max_rate = bitRateRange.second;
    }
}

void StreamCoder::setGlobalQuality(int32_t quality)
{
    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    if (m_context)
        m_context->global_quality = quality;
}

void StreamCoder::setGopSize(int32_t size)
{
    if (m_context)
        m_context->gop_size = size;
}

void StreamCoder::setBitRateTolerance(int bitRateTolerance)
{
    if (m_context)
        m_context->bit_rate_tolerance = bitRateTolerance;
}

void StreamCoder::setStrict(int strict)
{
    if (m_context)
    {
        if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        {
            strict = FF_COMPLIANCE_EXPERIMENTAL;
        }
        else if (strict > FF_COMPLIANCE_VERY_STRICT)
        {
            strict = FF_COMPLIANCE_VERY_STRICT;
        }

        m_context->strict_std_compliance = strict;
    }
}

void StreamCoder::setMaxBFrames(int maxBFrames)
{
    if (m_context)
        m_context->max_b_frames = maxBFrames;
}

int StreamCoder::getSampleRate() const
{
    return (m_context ? m_context->sample_rate : -1);
}

int StreamCoder::getChannels() const
{
    if (!m_context)
    {
        return 0;
    }

    if (m_context->channels)
    {
        return m_context->channels;
    }

    if (m_context->channel_layout)
    {
        return av_get_channel_layout_nb_channels(m_context->channel_layout);
    }

    return 0;
}

AVSampleFormat StreamCoder::getSampleFormat() const
{
    return (m_context ? m_context->sample_fmt : AV_SAMPLE_FMT_NONE);
}

uint64_t StreamCoder::getChannelLayout() const
{
    if (!m_context)
    {
        return 0;
    }

    if (m_context->channel_layout)
    {
        return m_context->channel_layout;
    }

    if (m_context->channels)
    {
        return av_get_default_channel_layout(m_context->channels);
    }

    return 0;
}

int StreamCoder::getAudioFrameSize() const
{
    int result = 0;
    if (m_codec && m_codec->getAVCodec()->type == AVMEDIA_TYPE_AUDIO)
    {
        if (m_context)
        {
            if (m_context->frame_size <= 1)
            {
                // Rats; some PCM encoders give a frame size of 1, which is too
                //small.  We pick a more sensible value.
                result = m_defaultAudioFrameSize;
            }
            else
            {
                result = m_context->frame_size;
            }
        }
    }

    return result;
}

int StreamCoder::getDefaultAudioFrameSize() const
{
    return m_defaultAudioFrameSize;
}

void StreamCoder::setSampleRate(int sampleRate)
{
    assert(m_context);

    if (!m_openedFlag)
    {
        int sr = guessValue(sampleRate, m_context->codec->supported_samplerates, EqualComparator<int>(0));
        clog << "========= Input sampleRate: " << sampleRate << ", guessed sample rate: " << sr << endl;

        if (sr > 0)
            m_context->sample_rate = sr;
    }
}

void StreamCoder::setChannels(int channels)
{
    if (m_context && !m_openedFlag && channels > 0)
    {
        m_context->channels = channels;

        // Make channels and channel_layout sync
        if (m_context->channel_layout == 0 ||
            av_get_channel_layout_nb_channels(m_context->channel_layout) != channels)
        {
            m_context->channel_layout = av_get_default_channel_layout(channels);
        }
    }
}

void StreamCoder::setSampleFormat(AVSampleFormat sampleFormat)
{
    if (m_context && !m_openedFlag && sampleFormat >= 0)
    {
        m_context->sample_fmt = sampleFormat;
    }
}

void StreamCoder::setChannelLayout(uint64_t layout)
{
    if (m_context && !m_openedFlag && layout > 0)
    {
        m_context->channel_layout = layout;

        // Make channels and channel_layout sync
        if (m_context->channels == 0 ||
            av_get_default_channel_layout(m_context->channels) != layout)
        {
            m_context->channels = av_get_channel_layout_nb_channels(layout);
        }
    }
}

void StreamCoder::setAudioFrameSize(int frameSize)
{
    if (m_context)
        m_context->frame_size = frameSize;
}

void StreamCoder::setDefaultAudioFrameSize(int frameSize)
{
    m_defaultAudioFrameSize = frameSize;
}

void StreamCoder::setFlags(int flags)
{
    if (m_context)
        m_context->flags = flags;
}

void StreamCoder::addFlags(int flags)
{
    if (m_context)
        m_context->flags |= flags;
}

void StreamCoder::clearFlags(int flags)
{
    if (m_context)
        m_context->flags &= ~flags;
}


int StreamCoder::getFlags()
{
    return (m_context ? m_context->flags : 0);
}


ssize_t StreamCoder::decodeVideo(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset)
{
    return decodeCommon(outFrame, inPacket, offset, avcodec_decode_video2);
}


ssize_t StreamCoder::encodeVideo(const VideoFramePtr &inFrame, const EncodedPacketHandler &onPacketHandler)
{
    if (inFrame->getPixelFormat() != getPixelFormat())
    {
        cerr << "Frame does not have same PixelFormat to coder: " << av_get_pix_fmt_name(inFrame->getPixelFormat()) << endl;
        return -1;
    }

    if (inFrame->getWidth() != getWidth())
    {
        //cerr << "Frame does not have same Width to coder\n";
        cerr << "Frame does not have same Width to coder: " << inFrame->getWidth() << " vs. " << getWidth() << '\n';
        return -1;
    }

    if (inFrame->getHeight() != getHeight())
    {
        cerr << "Frame does not have same Height to coder\n";
        return -1;
    }

    return encodeCommon(inFrame, avcodec_encode_video2, onPacketHandler);
}

ssize_t StreamCoder::decodeAudio(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset)
{
    return decodeCommon(outFrame, inPacket, offset, avcodec_decode_audio4);
}

ssize_t StreamCoder::encodeAudio(const FramePtr &inFrame, const EncodedPacketHandler &onPacketHandler)
{
    // TODO: additional checks like encodeVideo()
    return encodeCommon(inFrame, avcodec_encode_audio2, onPacketHandler);
}

bool StreamCoder::isValidForEncode()
{
    if (!m_openedFlag)
    {
        cerr << "You must open coder before encoding\n";
        return false;
    }

    if (!m_context)
    {
        cerr << "Codec context does not exists\n";
        return false;
    }

//    if (context->codec_type != AVMEDIA_TYPE_VIDEO)
//    {
//        cerr << "Attempting to encode video with non video coder\n";
//        return false;
//    }

    if (m_direction == DECODING)
    {
        cerr << "Decoding coder does not valid for encoding\n";
        return false;
    }

    if (!m_codec)
    {
        cerr << "Codec does not set\n";
        return false;
    }

    if (!m_codec->canEncode())
    {
        cerr << "Codec can't be used for Encode\n";
        return false;
    }

    m_fakeCurrPts = AV_NOPTS_VALUE;
    m_fakeNextPts = AV_NOPTS_VALUE;

    return true;
}


} // ::av
