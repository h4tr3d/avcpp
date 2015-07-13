#include <stdexcept>

#include "avlog.h"
#include "avutils.h"
#include <averror.h>

#include "codeccontext.h"

using namespace std;

namespace av {

#define warnIfNotVideo() { if (isValid() && !isVideo()) fflog(AV_LOG_WARNING, "Try to set parameter for video but context not for video\n"); }
#define warnIfNotAudio() { if (isValid() && !isAudio()) fflog(AV_LOG_WARNING, "Try to set parameter for audio but context not for audio\n"); }


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
    error_code ec;
    close(ec);
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

void CodecContext::setCodec(const Codec &codec, error_code &ec)
{
    setCodec(codec, false, ec);
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

void CodecContext::setCodec(const Codec &codec, bool resetDefaults, error_code &ec)
{
    clear_if(ec);

    if (!m_raw)
    {
        fflog(AV_LOG_WARNING, "Codec context does not allocated\n");
        throws_if(ec, AvError::Unallocated);
        return;
    }

    if (!m_raw || (!m_stream.isValid() && !m_stream.isNull()))
    {
        fflog(AV_LOG_WARNING, "Parent stream is not valid. Probably it or FormatContext destroyed\n");
        throws_if(ec, AvError::CodecStreamInvalid);
        return;
    }

    if (codec.isNull())
    {
        fflog(AV_LOG_WARNING, "Try to set null codec\n");
    }

    if (m_direction == Direction::ENCODING && codec.canEncode() == false)
    {
        fflog(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
        throws_if(ec, AvError::CodecInvalidDirection);
        return;
    }

    if (m_direction == Direction::DECODING && codec.canDecode() == false)
    {
        fflog(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
        throws_if(ec, AvError::CodecInvalidDirection);
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

void CodecContext::open(error_code &ec)
{
    open(Codec(), ec);
}

void CodecContext::open(const Codec &codec, std::error_code &ec)
{
    open(codec, nullptr, ec);
}

void CodecContext::open(Dictionary &options, error_code &ec)
{
    open(options, Codec(), ec);
}

void CodecContext::open(Dictionary &&options, error_code &ec)
{
    open(std::move(options), Codec(), ec);
}

void CodecContext::open(Dictionary &options, const Codec &codec, error_code &ec)
{
    auto prt = options.release();
    open(codec, &prt, ec);
    options.assign(prt);
}

void CodecContext::open(Dictionary &&options, const Codec &codec, error_code &ec)
{
    open(options, codec, ec);
}


void CodecContext::open(const Codec &codec, AVDictionary **options, error_code &ec)
{
    clear_if(ec);

    if (m_isOpened || !isValid()) {
        throws_if(ec, m_isOpened ? AvError::CodecAlreadyOpened : AvError::CodecInvalid);
        return;
    }

    int stat = avcodec_open2(m_raw, codec.isNull() ? m_raw->codec : codec.raw(), options);
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
    else
        m_isOpened = true;
}

void CodecContext::close(error_code &ec)
{
    clear_if(ec);
    if (m_isOpened)
    {
        if (isValid()) {
            avcodec_close(m_raw);
        }
        m_isOpened = false;
        return;
    }
    throws_if(ec, AvError::CodecDoesNotOpened);
}

bool CodecContext::isValid() const noexcept
{
    // Check parent stream first
    return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
}

void CodecContext::copyContextFrom(const CodecContext &other, error_code &ec)
{
    clear_if(ec);
    if (!isValid()) {
        fflog(AV_LOG_WARNING, "Invalid target context\n");
        throws_if(ec, AvError::CodecInvalid);
        return;
    }
    if (!other.isValid()) {
        fflog(AV_LOG_WARNING, "Invalid source context\n");
        throws_if(ec, AvError::CodecInvalid);
        return;
    }
    if (isOpened()) {
        fflog(AV_LOG_WARNING, "Try to copy context to opened target context\n");
        throws_if(ec, AvError::CodecAlreadyOpened);
        return;
    }
    if (this == &other) {
        fflog(AV_LOG_WARNING, "Same context\n");
        // No error here, simple do nothig
        return;
    }

    int stat = avcodec_copy_context(m_raw, other.m_raw);
    m_raw->codec_tag = 0;
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
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
            fflog(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type\n");

        return m_raw->codec_type;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}

void CodecContext::setOption(const string &key, const string &val, error_code &ec)
{
    setOption(key, val, 0, ec);
}

void CodecContext::setOption(const string &key, const string &val, int flags, error_code &ec)
{
    clear_if(ec);
    if (isValid())
    {
        auto sts = av_opt_set(m_raw->priv_data, key.c_str(), val.c_str(), flags);
        throws_if(ec, sts, ffmpeg_category());
    }
    else
    {
        throws_if(ec, AvError::CodecInvalid);
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
    {
        fflog(AV_LOG_INFO, "Guess sample rate %d instead unsupported %d\n", sr, sampleRate);
    }
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
        (uint64_t)av_get_default_channel_layout(m_raw->channels) != layout)
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

VideoFrame2 CodecContext::decodeVideo(const Packet &packet, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, 0, nullptr, autoAllocateFrame);
}

VideoFrame2 CodecContext::decodeVideo(const Packet &packet, size_t offset, size_t &decodedBytes, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, offset, &decodedBytes, autoAllocateFrame);
}

Packet CodecContext::encodeVideo(error_code &ec)
{
    return encodeVideo(VideoFrame2(nullptr), ec);
}

VideoFrame2 CodecContext::decodeVideo(error_code &ec, const Packet &packet, size_t offset, size_t *decodedBytes, bool autoAllocateFrame)
{
    clear_if(ec);

    VideoFrame2 outFrame;
    if (!autoAllocateFrame)
    {
        outFrame = {pixelFormat(), width(), height(), 32};

        if (!outFrame.isValid())
        {
            throws_if(ec, AvError::FrameInvalid);
            return VideoFrame2();
        }
    }

    int         gotFrame = 0;
    auto st = decodeCommon(outFrame, packet, offset, gotFrame, avcodec_decode_video2);

    if (get<0>(st) < 0) {
        throws_if(ec, get<0>(st), *get<1>(st));
        return VideoFrame2();
    }

    if (!gotFrame)
        return VideoFrame2();

    outFrame.setPictureType(AV_PICTURE_TYPE_I);

    if (decodedBytes)
        *decodedBytes = get<0>(st);

    return outFrame;
}


Packet CodecContext::encodeVideo(const VideoFrame2 &inFrame, error_code &ec)
{
    clear_if(ec);

    int gotPacket = 0;
    Packet packet;
    auto st = encodeCommon(packet, inFrame, gotPacket, avcodec_encode_video2);

    if (get<0>(st) < 0) {
        throws_if(ec, get<0>(st), *get<1>(st));
        return Packet();
    }

    if (!gotPacket) {
        packet.setComplete(false);
        return packet;
    }

    packet.setKeyPacket(!!m_raw->coded_frame->key_frame);

    return packet;

}

AudioSamples2 CodecContext::decodeAudio(const Packet &inPacket, error_code &ec)
{
    return decodeAudio(inPacket, 0, ec);
}

AudioSamples2 CodecContext::decodeAudio(const Packet &inPacket, size_t offset, error_code &ec)
{
    clear_if(ec);

    AudioSamples2 outSamples;

    int gotFrame = 0;
    auto st = decodeCommon(outSamples, inPacket, offset, gotFrame, avcodec_decode_audio4);
    if (get<0>(st) < 0)
    {
        throws_if(ec, get<0>(st), *get<1>(st));
        return AudioSamples2();
    }

    if (!gotFrame)
    {
        outSamples.setComplete(false);
        return outSamples;
    }

    return outSamples;

}

Packet CodecContext::encodeAudio(error_code &ec)
{
    return encodeAudio(AudioSamples2(nullptr), ec);
}

Packet CodecContext::encodeAudio(const AudioSamples2 &inSamples, error_code &ec)
{
    clear_if(ec);

    Packet outPacket;

    int gotFrame = 0;
    auto st = encodeCommon(outPacket, inSamples, gotFrame, avcodec_encode_audio2);
    if (get<0>(st) < 0)
    {
        throws_if(ec, get<0>(st), *get<1>(st));
        return Packet();
    }

    if (!gotFrame)
    {
        outPacket.setComplete(false);
        return outPacket;
    }

    // Fake PTS actual only for Audio
    if (inSamples.fakePts() != AV_NOPTS_VALUE) {
        outPacket.setFakePts(inSamples.fakePts(), inSamples.timeBase());
    }

    return outPacket;
}

bool CodecContext::isValidForEncode()
{
    if (!isValid())
    {
        fflog(AV_LOG_WARNING,
              "Not valid context: codec_context=%p, stream_valid=%d, stream_isnull=%d, codec=%p\n",
              m_raw,
              m_stream.isValid(),
              m_stream.isNull(),
              codec().raw());
        return false;
    }

    if (!m_isOpened)
    {
        fflog(AV_LOG_WARNING, "You must open coder before encoding\n");
        return false;
    }

    if (m_direction == Direction::DECODING)
    {
        fflog(AV_LOG_WARNING, "Decoding coder does not valid for encoding\n");
        return false;
    }

    if (!codec().canEncode())
    {
        fflog(AV_LOG_WARNING, "Codec can't be used for Encode\n");
        return false;
    }

    return true;
}

namespace {

std::pair<ssize_t, const error_category*>
make_error_pair(AvError errc)
{
    return make_pair(static_cast<ssize_t>(errc), &avcpp_category());
}

std::pair<ssize_t, const error_category*>
make_error_pair(ssize_t status)
{
    if (status < 0)
        return make_pair(status, &ffmpeg_category());
    return make_pair(status, nullptr);
}

}

std::pair<ssize_t, const error_category *> CodecContext::decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    if (!isValid())
        return make_error_pair(AvError::CodecInvalid);

    if (!isOpened())
        return make_error_pair(AvError::CodecDoesNotOpened);

    if (!decodeProc)
        return make_error_pair(AvError::CodecInvalidDecodeProc);

    if (offset >= inPacket.size())
        return make_error_pair(AvError::CodecDecodingOffsetToLarge);

    frameFinished = 0;

    AVPacket pkt = *inPacket.raw();
    pkt.data += offset;
    pkt.size -= offset;

    m_raw->reordered_opaque = inPacket.pts();

    int decoded = decodeProc(m_raw, outFrame, &frameFinished, &pkt);
    return make_error_pair(decoded);

#if 0
    // TODO: need to review
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
    return totalDecode;
#endif
}

std::pair<ssize_t, const error_category *> CodecContext::encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
{
    if (!isValid()) {
        fflog(AV_LOG_ERROR, "Invalid context\n");
        return make_error_pair(AvError::CodecInvalid);
    }

    if (!isValidForEncode()) {
        fflog(AV_LOG_ERROR, "Context can't be used for encoding\n");
        return make_error_pair(AvError::CodecInvalidForEncode);
    }

    if (!encodeProc) {
        fflog(AV_LOG_ERROR, "Encoding proc is null\n");
        return make_error_pair(AvError::CodecInvalidEncodeProc);
    }

    int stat = encodeProc(m_raw, outPacket.raw(), inFrame, &gotPacket);
    if (stat) {
        fflog(AV_LOG_ERROR, "Encode error: %d, %s\n", stat, error2string(stat).c_str());
    }
    return make_error_pair(stat);;
}

template<typename T>
std::pair<ssize_t, const error_category *> CodecContext::decodeCommon(T &outFrame, const Packet & inPacket, size_t offset, int & frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    auto st = decodeCommon(outFrame.raw(), inPacket, offset, frameFinished, decodeProc);
    if (get<0>(st) < 0)
        return st;

    if (!frameFinished)
        return make_pair(0u, nullptr);

    outFrame.setTimeBase(timeBase());
    outFrame.setStreamIndex(inPacket.streamIndex());
    AVFrame *frame = outFrame.raw();

    int64_t packetTs = frame->reordered_opaque;
    if (packetTs == AV_NOPTS_VALUE)
        packetTs = inPacket.dts();

    if (packetTs != AV_NOPTS_VALUE)
    {
        int64_t nextPts = packetTs;

        if (nextPts < m_fakeNextPts && inPacket.pts() != AV_NOPTS_VALUE)
        {
            nextPts = inPacket.pts();
        }

        m_fakeNextPts = nextPts;
    }

    m_fakeCurrPts = m_fakeNextPts;
    double frameDelay = inPacket.timeBase().getDouble();
    frameDelay += outFrame.raw()->repeat_pict * (frameDelay * 0.5);

    m_fakeNextPts += (int64_t) frameDelay;

    if (m_fakeCurrPts != AV_NOPTS_VALUE)
        outFrame.setPts(inPacket.timeBase().rescale(m_fakeCurrPts, outFrame.timeBase()));
    outFrame.setComplete(true);

    return st;
}

template<typename T>
std::pair<ssize_t, const error_category *> CodecContext::encodeCommon(Packet & outPacket, const T & inFrame, int & gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
{
    auto st = encodeCommon(outPacket, inFrame.raw(), gotPacket, encodeProc);
    if (get<0>(st) < 0)
        return st;
    if (!gotPacket)
        return make_pair(0u, nullptr);

    outPacket.setStreamIndex(inFrame.streamIndex());

    if (m_stream.isValid()) {
        outPacket.setTimeBase(m_stream.timeBase());
    } else {
        outPacket.setTimeBase(inFrame.timeBase());
    }

    if (m_raw->coded_frame) {
        if (m_raw->coded_frame->pts != AV_NOPTS_VALUE)
            outPacket.setPts(m_raw->coded_frame->pts, timeBase());
    } else {
        outPacket.setPts(inFrame.pts(), inFrame.timeBase());
    }

    if (outPacket.dts() != AV_NOPTS_VALUE && outPacket.pts() == AV_NOPTS_VALUE) {
        outPacket.setPts(outPacket.dts());
    }

    if (outPacket.dts() == AV_NOPTS_VALUE)
        outPacket.setDts(outPacket.pts());

    outPacket.setComplete(true);

    return st;
}


#undef warnIfNotAudio
#undef warnIfNotVideo

} // namespace av
