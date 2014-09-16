#include <stdexcept>

#include "avlog.h"
#include "avutils.h"

#include "codeccontext.h"

using namespace std;

namespace av {

#define warnIfNotVideo() { if (isValid() && !isVideo()) ptr_log(AV_LOG_WARNING, "Try to set parameter for video but context not for video\n"); }
#define warnIfNotAudio() { if (isValid() && !isAudio()) ptr_log(AV_LOG_WARNING, "Try to set parameter for audio but context not for audio\n"); }


CodecContext::CodecContext()
{
    m_raw = avcodec_alloc_context3(nullptr);
}

CodecContext::CodecContext(const Stream2 &st, const Codec &codec)
    : m_stream(st)
{
    m_raw = st.raw()->codec;

    Codec c = codec;

    if (codec.isNull()) {
        if (st.direction() == Direction::DECODING)
            c = findDecodingCodec(m_raw->codec_id);
        else
            c = findEncodingCodec(m_raw->codec_id);
    }

    if (!c.isNull())
        setCodec(c);

    m_direction = st.direction();
}

CodecContext::CodecContext(const Codec &codec)
{
    m_raw = avcodec_alloc_context3(codec.raw());
}

CodecContext::~CodecContext()
{
    close();
    if (m_stream.isNull())
        av_freep(&m_raw);
}


CodecContext::CodecContext(CodecContext &&other)
    : CodecContext()
{
    swap(other);
}

CodecContext &CodecContext::operator=(CodecContext &&rhs)
{
    if (this == &rhs)
        return *this;

    CodecContext(std::move(rhs)).swap(*this);
    return *this;
}

void CodecContext::swap(CodecContext &other)
{
    using std::swap;
    swap(m_direction, other.m_direction);
    swap(m_fakePtsTimeBase, other.m_fakePtsTimeBase);
    swap(m_fakeCurrPts, other.m_fakeCurrPts);
    swap(m_fakeNextPts, other.m_fakeNextPts);
    swap(m_stream, other.m_stream);
    swap(m_raw, other.m_raw);
    swap(m_isOpened, other.m_isOpened);
}

void CodecContext::setCodec(const Codec &codec, bool resetDefaults)
{
    if (!m_raw || (!m_stream.isValid() && !m_stream.isNull()))
    {
        null_log(AV_LOG_WARNING, "Parent stream is not valid. Probably it or FormatContext destrayed\n");
        return;
    }

    if (codec.isNull())
    {
        ptr_log(AV_LOG_WARNING, "Try to set null codec\n");
    }

    if (!m_raw)
    {
        ptr_log(AV_LOG_WARNING, "Codec context does not allocated\n");
        return;
    }

    if (m_direction == Direction::ENCODING && codec.canEncode() == false)
    {
        ptr_log(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
        return;
    }

    if (m_direction == Direction::DECODING && codec.canDecode() == false)
    {
        ptr_log(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
        return;
    }

    if (resetDefaults) {
        if (!m_raw->codec) {
            avcodec_free_context(&m_raw);
            m_raw = avcodec_alloc_context3(codec.raw());
        } else {
            avcodec_get_context_defaults3(m_raw, codec.raw());
        }
    } else {
        m_raw->codec_id   = !codec.isNull() ? codec.raw()->id : CODEC_ID_NONE;
        m_raw->codec_type = !codec.isNull() ? codec.raw()->type : AVMEDIA_TYPE_UNKNOWN;
        m_raw->codec      = codec.raw();

        if (!codec.isNull()) {
            if (codec.raw()->pix_fmts != 0)
                m_raw->pix_fmt = *(codec.raw()->pix_fmts); // assign default value
            if (codec.raw()->sample_fmts != 0)
                m_raw->sample_fmt = *(codec.raw()->sample_fmts);
        }
    }

    if (m_stream.isValid()) {
        m_stream.raw()->codec = m_raw;
    }
}

bool CodecContext::open(const Codec &codec)
{
    if (m_isOpened || !isValid())
        return false;

    // HACK: set threads to 1
    AVDictionary *opts = 0;
    av_dict_set(&opts, "threads", "1", 0);
    ////////////////////////////////////////////////////////////////////////////////////////////

    bool ret = true;
    int stat = avcodec_open2(m_raw, codec.isNull() ? m_raw->codec : codec.raw(), &opts);
    if (stat < 0)
        ret = false;
    else
        m_isOpened = true;

    av_dict_free(&opts);

    return ret;
}

bool CodecContext::close()
{
    if (m_isOpened)
    {
        if (isValid()) {
                avcodec_close(m_raw);
        }
        m_isOpened = false;
        return true;
    }


    return false;
}

bool CodecContext::isValid() const
{
    // Check parent stream first
    return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
}

bool CodecContext::copyContextFrom(const CodecContext &other)
{
    if (!isValid()) {
        null_log(AV_LOG_WARNING, "Invalid target context\n");
        return false;
    }
    if (!other.isValid()) {
        null_log(AV_LOG_WARNING, "Invalid source context\n");
        return false;
    }
    if (isOpened()) {
        ptr_log(AV_LOG_WARNING, "Try to copy context to opened target context\n");
        return false;
    }
    if (this == &other) {
        ptr_log(AV_LOG_WARNING, "Same context\n");
        return false;
    }

    int stat = avcodec_copy_context(m_raw, other.m_raw);
    m_raw->codec_tag = 0;

    return stat < 0 ? false : true;
}

Rational CodecContext::timeBase() const
{
    return RAW_GET2(isValid(), time_base, Rational());
}

void CodecContext::setTimeBase(const Rational &value)
{
    RAW_SET2(isValid(), time_base, value.getValue());
}

const Stream2 &CodecContext::stream() const
{
    return m_stream;
}

Codec CodecContext::codec() const
{
    if (isValid())
        return Codec(m_raw->codec);
    else
        return Codec();
}

AVMediaType CodecContext::codecType() const
{
    if (isValid())
    {
        if (m_raw->codec && m_raw->codec_type != m_raw->codec->type)
            ptr_log(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type\n");

        return m_raw->codec_type;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}

void CodecContext::setOption(const string &key, const string &val, int flags)
{
    if (isValid())
    {
        av_opt_set(m_raw->priv_data, key.c_str(), val.c_str(), flags);
    }
}

bool CodecContext::isAudio() const
{
    return codecType() == AVMEDIA_TYPE_AUDIO;
}

bool CodecContext::isVideo() const
{
    return codecType() == AVMEDIA_TYPE_VIDEO;
}

int CodecContext::frameSize() const
{
    return RAW_GET2(isValid(), frame_size, 0);
}

int CodecContext::frameNumber() const
{
    return RAW_GET2(isValid(), frame_number, 0);
}

bool CodecContext::isRefCountedFrames() const
{
    return RAW_GET2(isValid(), refcounted_frames, false);
}

void CodecContext::setRefCountedFrames(bool refcounted) const
{
    RAW_SET2(isValid(), refcounted_frames, refcounted);
}

int CodecContext::width() const
{
    return RAW_GET2(isValid(), width, 0);
}

int CodecContext::height() const
{
    return RAW_GET2(isValid(), height, 0);
}

int CodecContext::codedWidth() const
{
    return RAW_GET2(isValid(), coded_width, 0);
}

int CodecContext::codedHeight() const
{
    return RAW_GET2(isValid(), coded_height, 0);
}

AVPixelFormat CodecContext::pixelFormat() const
{
    return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
}

int32_t CodecContext::bitRate() const
{
    return RAW_GET2(isValid(), bit_rate, 0);
}

std::pair<int, int> CodecContext::bitRateRange() const
{
    if (isValid())
        return make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
    else
        return make_pair(0, 0);
}

int32_t CodecContext::globalQuality()
{
    return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
}

int32_t CodecContext::gopSize()
{
    return RAW_GET2(isValid(), gop_size, 0);
}

int CodecContext::bitRateTolerance() const
{
    return RAW_GET2(isValid(), bit_rate_tolerance, 0);
}

int CodecContext::strict() const
{
    return RAW_GET2(isValid(), strict_std_compliance, 0);
}

int CodecContext::maxBFrames() const
{
    return RAW_GET2(isValid(), max_b_frames, 0);
}

void CodecContext::setWidth(int w)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->width       = w;
        m_raw->coded_width = w;
    }
}

void CodecContext::setHeight(int h)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->height       = h;
        m_raw->coded_height = h;
    }
}

void CodecContext::setCodedWidth(int w)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, coded_width, w);
}

void CodecContext::setCodedHeight(int h)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, coded_height, h);
}

void CodecContext::setPixelFormat(AVPixelFormat pixelFormat)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, pix_fmt, pixelFormat);
}

void CodecContext::setBitRate(int32_t bitRate)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, bit_rate, bitRate);
}

void CodecContext::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->rc_min_rate = get<0>(bitRateRange);
        m_raw->rc_max_rate = get<1>(bitRateRange);
    }
}

void CodecContext::setGlobalQuality(int32_t quality)
{
    warnIfNotVideo();
    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    RAW_SET2(isValid() && !m_isOpened, global_quality, quality);
}

void CodecContext::setGopSize(int32_t size)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, gop_size, size);
}

void CodecContext::setBitRateTolerance(int bitRateTolerance)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, bit_rate_tolerance, bitRateTolerance);
}

void CodecContext::setStrict(int strict)
{
    warnIfNotVideo();
    if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        strict = FF_COMPLIANCE_EXPERIMENTAL;
    else if (strict > FF_COMPLIANCE_VERY_STRICT)
        strict = FF_COMPLIANCE_VERY_STRICT;

    RAW_SET2(isValid() && !m_isOpened, strict_std_compliance, strict);
}

void CodecContext::setMaxBFrames(int maxBFrames)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, max_b_frames, maxBFrames);
}

int CodecContext::sampleRate() const
{
    warnIfNotVideo();
    return RAW_GET2(isValid(), sample_rate, 0);
}

int CodecContext::channels() const
{
    if (!isValid())
        return 0;

    if (m_raw->channels)
        return m_raw->channels;

    if (m_raw->channel_layout)
        return av_get_channel_layout_nb_channels(m_raw->channel_layout);

    return 0;
}

AVSampleFormat CodecContext::sampleFormat() const
{
    return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
}

uint64_t CodecContext::channelLayout() const
{
    if (!isValid())
        return 0;

    if (m_raw->channel_layout)
        return m_raw->channel_layout;

    if (m_raw->channels)
        return av_get_default_channel_layout(m_raw->channels);

    return 0;
}

void CodecContext::setSampleRate(int sampleRate)
{
    warnIfNotAudio();
    if (!isValid() || m_isOpened)
        return;
    int sr = guessValue(sampleRate, m_raw->codec->supported_samplerates, EqualComparator<int>(0));
    if (sr != sampleRate)
        ptr_log(AV_LOG_INFO, "Guess sample rate %d instead unsupported %d\n", sr, sampleRate);
    if (sr > 0)
        m_raw->sample_rate = sr;
}

void CodecContext::setChannels(int channels)
{
    if (!isValid() || channels <= 0 || m_isOpened)
        return;
    m_raw->channels = channels;
    if (m_raw->channel_layout != 0 ||
        av_get_channel_layout_nb_channels(m_raw->channel_layout) != channels) {
        m_raw->channel_layout = av_get_default_channel_layout(channels);
    }
}

void CodecContext::setSampleFormat(AVSampleFormat sampleFormat)
{
    warnIfNotAudio();
    RAW_SET2(isValid() && !m_isOpened, sample_fmt, sampleFormat);
}

void CodecContext::setChannelLayout(uint64_t layout)
{
    warnIfNotAudio();
    if (!isValid() || m_isOpened || layout == 0)
        return;

    m_raw->channel_layout = layout;

    // Make channels and channel_layout sync
    if (m_raw->channels == 0 ||
        av_get_default_channel_layout(m_raw->channels) != layout)
    {
        m_raw->channels = av_get_channel_layout_nb_channels(layout);
    }
}

void CodecContext::setFlags(int flags)
{
    RAW_SET2(isValid() && !m_isOpened, flags, flags);
}

void CodecContext::addFlags(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags |= flags;
}

void CodecContext::clearFlags(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags &= ~flags;
}

int CodecContext::flags()
{
    return RAW_GET2(isValid() && !m_isOpened, flags, 0);
}

bool CodecContext::isFlags(int flags)
{
    if (isValid() && !m_isOpened)
        return (m_raw->flags & flags);
    return false;
}

//
void CodecContext::setFlags2(int flags)
{
    RAW_SET2(isValid() && !m_isOpened, flags2, flags);
}

void CodecContext::addFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags2 |= flags;
}

void CodecContext::clearFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags2 &= ~flags;
}

int CodecContext::flags2()
{
    return RAW_GET2(isValid() && !m_isOpened, flags2, 0);
}

bool CodecContext::isFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        return (m_raw->flags2 & flags);
    return false;
}

ssize_t CodecContext::decodeVideo(VideoFrame2 &outFrame, const Packet &inPacket, size_t offset)
{
    int gotFrame = 0;
    int st = decodeCommon(outFrame.raw(), inPacket, offset, gotFrame, avcodec_decode_video2);

    if (st < 0)
        return st;

    if (!gotFrame)
        return 0;

    outFrame.setTimeBase(timeBase());
    outFrame.setStreamIndex(inPacket.getStreamIndex());
    outFrame.setPictureType(AV_PICTURE_TYPE_I);
    AVFrame *frame = outFrame.raw();

    int64_t packetTs = frame->reordered_opaque;
    if (packetTs == AV_NOPTS_VALUE)
        packetTs = inPacket.getDts();

    if (packetTs != AV_NOPTS_VALUE)
    {
        int64_t nextPts = packetTs;

        if (nextPts < m_fakeNextPts && inPacket.getPts() != AV_NOPTS_VALUE)
        {
            nextPts = inPacket.getPts();
        }

        m_fakeNextPts = nextPts;
    }

    m_fakeCurrPts = m_fakeNextPts;
    double frameDelay = inPacket.getTimeBase().getDouble();
    frameDelay += outFrame.raw()->repeat_pict * (frameDelay * 0.5);

    m_fakeNextPts += (int64_t) frameDelay;

    if (m_fakeCurrPts != AV_NOPTS_VALUE)
        outFrame.setPts(inPacket.getTimeBase().rescale(m_fakeCurrPts, outFrame.timeBase()));
    outFrame.setComplete(true);
}

ssize_t CodecContext::encodeVideo(Packet &outPacket, const VideoFrame2 &inFrame)
{
    int gotPacket = 0;
    int st = encodeCommon(outPacket, inFrame.raw(), gotPacket, avcodec_encode_video2);

    if (st < 0)
        return st;

    if (!gotPacket)
        return 0;

    outPacket.setKeyPacket(!!m_raw->coded_frame->key_frame);
    outPacket.setStreamIndex(inFrame.streamIndex());

    if (m_stream.isValid()) {
        outPacket.setTimeBase(m_stream.timeBase());
    } else {
        outPacket.setTimeBase(inFrame.timeBase());
    }

    if (m_raw->coded_frame->pts != AV_NOPTS_VALUE)
        outPacket.setPts(m_raw->coded_frame->pts, timeBase());
    if (outPacket.getDts() == AV_NOPTS_VALUE)
        outPacket.setDts(outPacket.getPts());
}

bool CodecContext::isValidForEncode()
{
    if (!isValid())
    {
        null_log(AV_LOG_WARNING,
                 "Not valid context: codec_context=%p, stream_valid=%d, stream_isnull=%d, codec=%p\n",
                 m_raw,
                 m_stream.isValid(),
                 m_stream.isNull(),
                 codec().raw());
        return false;
    }

    if (!m_isOpened)
    {
        ptr_log(AV_LOG_WARNING, "You must open coder before encoding\n");
        return false;
    }

    if (m_direction == Direction::DECODING)
    {
        ptr_log(AV_LOG_WARNING, "Decoding coder does not valid for encoding\n");
        return false;
    }

    if (!codec().canEncode())
    {
        ptr_log(AV_LOG_WARNING, "Codec can't be used for Encode\n");
        return false;
    }

    return true;
}

ssize_t CodecContext::decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    if (!isValid())
        return -1;

    if (!isOpened())
        return -1;

    if (!decodeProc)
    {
        return -1;
    }

    if (offset >= inPacket.getSize())
    {
        return -1;
    }

    frameFinished = 0;

    AVPacket pkt = *inPacket.raw();
    pkt.data += offset;
    pkt.size -= offset;

    m_raw->reordered_opaque = inPacket.getPts();
    ssize_t totalDecode = 0;
    do
    {
        int decoded = decodeProc(m_raw, outFrame, &frameFinished, &pkt);
        if (decoded < 0)
        {
            return totalDecode;
        }

        totalDecode += decoded;
        pkt.data   += decoded;
        pkt.size   -= decoded;
    }
    while (frameFinished == 0 && pkt.size > 0);

#if 0
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
        double frameDelay = inPacket->getTimeBase().getDouble();
        frameDelay += outFrame->getAVFrame()->repeat_pict * (frameDelay * 0.5);

        m_fakeNextPts += (int64_t) frameDelay;

        outFrame->setStreamIndex(inPacket->getStreamIndex());
        if (m_fakeCurrPts != AV_NOPTS_VALUE)
            outFrame->setPts(inPacket->getTimeBase().rescale(m_fakeCurrPts, outFrame->getTimeBase()));
        outFrame->setComplete(true);
    }
#endif

    return totalDecode;
}

ssize_t CodecContext::encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
{
    if (!isValid())
        return -1;

    if (!encodeProc)
        return -1;

    int stat = encodeProc(m_raw, outPacket.raw(), inFrame, &gotPacket);
    if (stat != 0) {
        ptr_log(AV_LOG_ERROR,
                "Encode error: %d\n",
                stat);
    }
    return stat;
}


#undef warnIfNotAudio
#undef warnIfNotVideo

} // namespace av
