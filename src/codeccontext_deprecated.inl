#include <stdexcept>

#include "avlog.h"
#include "avutils.h"
#include "averror.h"

#include "codeccontext_deprecated.h"

using namespace std;

namespace av {

#define warnIfNotVideo() { if (isValid() && !isVideo()) fflog(AV_LOG_WARNING, "Try to set parameter for video but context not for video\n"); }
#define warnIfNotAudio() { if (isValid() && !isAudio()) fflog(AV_LOG_WARNING, "Try to set parameter for audio but context not for audio\n"); }


CodecContextDeprecated::CodecContextDeprecated()
{
    m_raw = avcodec_alloc_context3(nullptr);
}

CodecContextDeprecated::CodecContextDeprecated(const Stream &st, const Codec &codec)
    : m_stream(st)
{

    Codec c = codec;

#if !defined(FF_API_LAVF_AVCTX)
    auto const codecId = st.raw()->codec->codec_id;
#else
    auto const codecId = st.raw()->codecpar->codec_id;
#endif

    if (codec.isNull())
    {
        if (st.direction() == Direction::Decoding)
            c = findDecodingCodec(codecId);
        else
            c = findEncodingCodec(codecId);
    }


#if !defined(FF_API_LAVF_AVCTX)
    m_raw = st.raw()->codec;
#else
    m_raw = avcodec_alloc_context3(c.raw());
#endif

    if (!c.isNull())
        setCodec(c);

    m_direction = st.direction();
}

CodecContextDeprecated::CodecContextDeprecated(const Codec &codec)
{
    m_raw = avcodec_alloc_context3(codec.raw());
}

CodecContextDeprecated::~CodecContextDeprecated()
{
#if !defined(FF_API_LAVF_AVCTX)
    if (m_stream.isNull())
        return;
#endif

    error_code ec;
    close(ec);
    av_freep(&m_raw);
}


CodecContextDeprecated::CodecContextDeprecated(CodecContextDeprecated &&other)
    : CodecContextDeprecated()
{
    swap(other);
}

CodecContextDeprecated &CodecContextDeprecated::operator=(CodecContextDeprecated &&rhs)
{
    if (this == &rhs)
        return *this;

    CodecContextDeprecated(std::move(rhs)).swap(*this);
    return *this;
}

void CodecContextDeprecated::setCodec(const Codec &codec, OptionalErrorCode ec)
{
    setCodec(codec, false, ec);
}

void CodecContextDeprecated::swap(CodecContextDeprecated &other)
{
    using std::swap;
    swap(m_direction, other.m_direction);
    swap(m_stream, other.m_stream);
    swap(m_raw, other.m_raw);
}

void CodecContextDeprecated::setCodec(const Codec &codec, bool resetDefaults, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw)
    {
        fflog(AV_LOG_WARNING, "Codec context does not allocated\n");
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (!m_raw || (!m_stream.isValid() && !m_stream.isNull()))
    {
        fflog(AV_LOG_WARNING, "Parent stream is not valid. Probably it or FormatContext destroyed\n");
        throws_if(ec, Errors::CodecStreamInvalid);
        return;
    }

    if (codec.isNull())
    {
        fflog(AV_LOG_WARNING, "Try to set null codec\n");
    }

    if (m_direction == Direction::Encoding && codec.canEncode() == false)
    {
        fflog(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
        throws_if(ec, Errors::CodecInvalidDirection);
        return;
    }

    if (m_direction == Direction::Decoding && codec.canDecode() == false)
    {
        fflog(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
        throws_if(ec, Errors::CodecInvalidDirection);
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
        m_raw->codec_id   = !codec.isNull() ? codec.raw()->id : AV_CODEC_ID_NONE;
        m_raw->codec_type = !codec.isNull() ? codec.raw()->type : AVMEDIA_TYPE_UNKNOWN;
        m_raw->codec      = codec.raw();

        if (!codec.isNull()) {
            if (codec.raw()->pix_fmts != 0)
                m_raw->pix_fmt = *(codec.raw()->pix_fmts); // assign default value
            if (codec.raw()->sample_fmts != 0)
                m_raw->sample_fmt = *(codec.raw()->sample_fmts);
        }
    }

#if !defined(FF_API_LAVF_AVCTX) // AVFORMAT < 57.5.0
    if (m_stream.isValid()) {
        m_stream.raw()->codec = m_raw;
    }
#else
    avcodec_parameters_from_context(m_stream.raw()->codecpar, m_raw);
#endif
}

void CodecContextDeprecated::open(OptionalErrorCode ec)
{
    open(Codec(), ec);
}

void CodecContextDeprecated::open(const Codec &codec, OptionalErrorCode ec)
{
    open(codec, nullptr, ec);
}

void CodecContextDeprecated::open(Dictionary &options, OptionalErrorCode ec)
{
    open(options, Codec(), ec);
}

void CodecContextDeprecated::open(Dictionary &&options, OptionalErrorCode ec)
{
    open(std::move(options), Codec(), ec);
}

void CodecContextDeprecated::open(Dictionary &options, const Codec &codec, OptionalErrorCode ec)
{
    auto prt = options.release();
    open(codec, &prt, ec);
    options.assign(prt);
}

void CodecContextDeprecated::open(Dictionary &&options, const Codec &codec, OptionalErrorCode ec)
{
    open(options, codec, ec);
}


void CodecContextDeprecated::open(const Codec &codec, AVDictionary **options, OptionalErrorCode ec)
{
    clear_if(ec);

    if (isOpened() || !isValid()) {
        throws_if(ec, isOpened() ? Errors::CodecAlreadyOpened : Errors::CodecInvalid);
        return;
    }

    int stat = avcodec_open2(m_raw, codec.isNull() ? m_raw->codec : codec.raw(), options);
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
}

void CodecContextDeprecated::close(OptionalErrorCode ec)
{
    clear_if(ec);
    if (isOpened())
    {
        if (isValid()) {
            avcodec_close(m_raw);
        }
        return;
    }
    throws_if(ec, Errors::CodecNotOpened);
}

bool CodecContextDeprecated::isOpened() const noexcept
{
    return m_raw ? avcodec_is_open(m_raw) : false;
}

bool CodecContextDeprecated::isValid() const noexcept
{
    // Check parent stream first
    return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
}

void CodecContextDeprecated::copyContextFrom(const CodecContextDeprecated &other, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!isValid()) {
        fflog(AV_LOG_WARNING, "Invalid target context\n");
        throws_if(ec, Errors::CodecInvalid);
        return;
    }
    if (!other.isValid()) {
        fflog(AV_LOG_WARNING, "Invalid source context\n");
        throws_if(ec, Errors::CodecInvalid);
        return;
    }
    if (isOpened()) {
        fflog(AV_LOG_WARNING, "Try to copy context to opened target context\n");
        throws_if(ec, Errors::CodecAlreadyOpened);
        return;
    }
    if (this == &other) {
        fflog(AV_LOG_WARNING, "Same context\n");
        // No error here, simple do nothig
        return;
    }

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(57,5,0)
    int stat = avcodec_copy_context(m_raw, other.m_raw);
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
#else
    AVCodecParameters params{};
    avcodec_parameters_from_context(&params, other.m_raw);
    avcodec_parameters_to_context(m_raw, &params);
#endif
    m_raw->codec_tag = 0;
}

Rational CodecContextDeprecated::timeBase() const
{
    return RAW_GET2(isValid(), time_base, AVRational{});
}

void CodecContextDeprecated::setTimeBase(const Rational &value)
{
    RAW_SET2(isValid(), time_base, value.getValue());
}

const Stream &CodecContextDeprecated::stream() const
{
    return m_stream;
}

Codec CodecContextDeprecated::codec() const
{
    if (isValid())
        return Codec(m_raw->codec);
    else
        return Codec();
}

AVMediaType CodecContextDeprecated::codecType() const
{
    if (isValid())
    {
        if (m_raw->codec && m_raw->codec_type != m_raw->codec->type)
            fflog(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type\n");

        return m_raw->codec_type;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}

void CodecContextDeprecated::setOption(const string &key, const string &val, OptionalErrorCode ec)
{
    setOption(key, val, 0, ec);
}

void CodecContextDeprecated::setOption(const string &key, const string &val, int flags, OptionalErrorCode ec)
{
    clear_if(ec);
    if (isValid())
    {
        auto sts = av_opt_set(m_raw->priv_data, key.c_str(), val.c_str(), flags);
        throws_if(ec, sts, ffmpeg_category());
    }
    else
    {
        throws_if(ec, Errors::CodecInvalid);
    }
}

bool CodecContextDeprecated::isAudio() const
{
    return codecType() == AVMEDIA_TYPE_AUDIO;
}

bool CodecContextDeprecated::isVideo() const
{
    return codecType() == AVMEDIA_TYPE_VIDEO;
}

int CodecContextDeprecated::frameSize() const
{
    return RAW_GET2(isValid(), frame_size, 0);
}

int CodecContextDeprecated::frameNumber() const
{
    return RAW_GET2(isValid(), frame_number, 0);
}

bool CodecContextDeprecated::isRefCountedFrames() const noexcept
{
    return RAW_GET2(isValid(), refcounted_frames, false);
}

void CodecContextDeprecated::setRefCountedFrames(bool refcounted) const noexcept
{
    RAW_SET2(isValid() && !isOpened(), refcounted_frames, refcounted);
}

int CodecContextDeprecated::width() const
{
    return RAW_GET2(isValid(), width, 0);
}

int CodecContextDeprecated::height() const
{
    return RAW_GET2(isValid(), height, 0);
}

int CodecContextDeprecated::codedWidth() const
{
    return RAW_GET2(isValid(), coded_width, 0);
}

int CodecContextDeprecated::codedHeight() const
{
    return RAW_GET2(isValid(), coded_height, 0);
}

PixelFormat CodecContextDeprecated::pixelFormat() const
{
    return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
}

int32_t CodecContextDeprecated::bitRate() const
{
    return RAW_GET2(isValid(), bit_rate, int32_t(0));
}

std::pair<int, int> CodecContextDeprecated::bitRateRange() const
{
    if (isValid())
        return make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
    else
        return make_pair(0, 0);
}

int32_t CodecContextDeprecated::globalQuality()
{
    return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
}

int32_t CodecContextDeprecated::gopSize()
{
    return RAW_GET2(isValid(), gop_size, 0);
}

int CodecContextDeprecated::bitRateTolerance() const
{
    return RAW_GET2(isValid(), bit_rate_tolerance, 0);
}

int CodecContextDeprecated::strict() const
{
    return RAW_GET2(isValid(), strict_std_compliance, 0);
}

int CodecContextDeprecated::maxBFrames() const
{
    return RAW_GET2(isValid(), max_b_frames, 0);
}

void CodecContextDeprecated::setWidth(int w)
{
    warnIfNotVideo();
    if (isValid())
    {
        m_raw->width       = w;
        m_raw->coded_width = w;
    }
}

void CodecContextDeprecated::setHeight(int h)
{
    warnIfNotVideo();
    if (isValid())
    {
        m_raw->height       = h;
        m_raw->coded_height = h;
    }
}

void CodecContextDeprecated::setCodedWidth(int w)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), coded_width, w);
}

void CodecContextDeprecated::setCodedHeight(int h)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), coded_height, h);
}

void CodecContextDeprecated::setPixelFormat(PixelFormat pixelFormat)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), pix_fmt, pixelFormat);
}

void CodecContextDeprecated::setBitRate(int32_t bitRate)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), bit_rate, bitRate);
}

void CodecContextDeprecated::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    warnIfNotVideo();
    if (isValid())
    {
        m_raw->rc_min_rate = get<0>(bitRateRange);
        m_raw->rc_max_rate = get<1>(bitRateRange);
    }
}

void CodecContextDeprecated::setGlobalQuality(int32_t quality)
{
    warnIfNotVideo();
    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    RAW_SET2(isValid(), global_quality, quality);
}

void CodecContextDeprecated::setGopSize(int32_t size)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), gop_size, size);
}

void CodecContextDeprecated::setBitRateTolerance(int bitRateTolerance)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), bit_rate_tolerance, bitRateTolerance);
}

void CodecContextDeprecated::setStrict(int strict)
{
    warnIfNotVideo();
    if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        strict = FF_COMPLIANCE_EXPERIMENTAL;
    else if (strict > FF_COMPLIANCE_VERY_STRICT)
        strict = FF_COMPLIANCE_VERY_STRICT;

    RAW_SET2(isValid(), strict_std_compliance, strict);
}

void CodecContextDeprecated::setMaxBFrames(int maxBFrames)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), max_b_frames, maxBFrames);
}

int CodecContextDeprecated::sampleRate() const
{
    return RAW_GET2(isValid(), sample_rate, 0);
}

int CodecContextDeprecated::channels() const
{
    if (!isValid())
        return 0;

    if (m_raw->channels)
        return m_raw->channels;

    if (m_raw->channel_layout)
        return av_get_channel_layout_nb_channels(m_raw->channel_layout);

    return 0;
}

SampleFormat CodecContextDeprecated::sampleFormat() const
{
    return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
}

uint64_t CodecContextDeprecated::channelLayout() const
{
    if (!isValid())
        return 0;

    if (m_raw->channel_layout)
        return m_raw->channel_layout;

    if (m_raw->channels)
        return av_get_default_channel_layout(m_raw->channels);

    return 0;
}

void CodecContextDeprecated::setSampleRate(int sampleRate)
{
    warnIfNotAudio();
    if (!isValid())
        return;
    int sr = guessValue(sampleRate, m_raw->codec->supported_samplerates, EqualComparator<int>(0));
    if (sr != sampleRate)
    {
        fflog(AV_LOG_INFO, "Guess sample rate %d instead unsupported %d\n", sr, sampleRate);
    }
    if (sr > 0)
        m_raw->sample_rate = sr;
}

void CodecContextDeprecated::setChannels(int channels)
{
    if (!isValid() || channels <= 0)
        return;
    m_raw->channels = channels;
    if (m_raw->channel_layout != 0 ||
        av_get_channel_layout_nb_channels(m_raw->channel_layout) != channels) {
        m_raw->channel_layout = av_get_default_channel_layout(channels);
    }
}

void CodecContextDeprecated::setSampleFormat(SampleFormat sampleFormat)
{
    warnIfNotAudio();
    RAW_SET2(isValid(), sample_fmt, sampleFormat);
}

void CodecContextDeprecated::setChannelLayout(uint64_t layout)
{
    warnIfNotAudio();
    if (!isValid() || layout == 0)
        return;

    m_raw->channel_layout = layout;

    // Make channels and channel_layout sync
    if (m_raw->channels == 0 ||
        (uint64_t)av_get_default_channel_layout(m_raw->channels) != layout)
    {
        m_raw->channels = av_get_channel_layout_nb_channels(layout);
    }
}

void CodecContextDeprecated::setFlags(int flags)
{
    RAW_SET2(isValid(), flags, flags);
}

void CodecContextDeprecated::addFlags(int flags)
{
    if (isValid())
        m_raw->flags |= flags;
}

void CodecContextDeprecated::clearFlags(int flags)
{
    if (isValid())
        m_raw->flags &= ~flags;
}

int CodecContextDeprecated::flags()
{
    return RAW_GET2(isValid(), flags, 0);
}

bool CodecContextDeprecated::isFlags(int flags)
{
    if (isValid())
        return (m_raw->flags & flags);
    return false;
}

//
void CodecContextDeprecated::setFlags2(int flags)
{
    RAW_SET2(isValid(), flags2, flags);
}

void CodecContextDeprecated::addFlags2(int flags)
{
    if (isValid())
        m_raw->flags2 |= flags;
}

void CodecContextDeprecated::clearFlags2(int flags)
{
    if (isValid())
        m_raw->flags2 &= ~flags;
}

int CodecContextDeprecated::flags2()
{
    return RAW_GET2(isValid(), flags2, 0);
}

bool CodecContextDeprecated::isFlags2(int flags)
{
    if (isValid())
        return (m_raw->flags2 & flags);
    return false;
}

VideoFrame CodecContextDeprecated::decodeVideo(const Packet &packet, OptionalErrorCode ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, 0, nullptr, autoAllocateFrame);
}

VideoFrame CodecContextDeprecated::decodeVideo(const Packet &packet, size_t offset, size_t &decodedBytes, OptionalErrorCode ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, offset, &decodedBytes, autoAllocateFrame);
}

Packet CodecContextDeprecated::encodeVideo(OptionalErrorCode ec)
{
    return encodeVideo(VideoFrame(nullptr), ec);
}

VideoFrame CodecContextDeprecated::decodeVideo(OptionalErrorCode ec, const Packet &packet, size_t offset, size_t *decodedBytes, bool autoAllocateFrame)
{
    clear_if(ec);

    VideoFrame outFrame;
    if (!autoAllocateFrame)
    {
        outFrame = {pixelFormat(), width(), height(), 32};

        if (!outFrame.isValid())
        {
            throws_if(ec, Errors::FrameInvalid);
            return VideoFrame();
        }
    }

    int gotFrame = 0;
    auto st = decodeCommon(outFrame, packet, offset, gotFrame, avcodec_decode_video_legacy);

    if (get<1>(st)) {
        throws_if(ec, get<0>(st), *get<1>(st));
        return VideoFrame();
    }

    if (!gotFrame)
        return VideoFrame();

    outFrame.setPictureType(AV_PICTURE_TYPE_I);

    if (decodedBytes)
        *decodedBytes = get<0>(st);

    return outFrame;
}


Packet CodecContextDeprecated::encodeVideo(const VideoFrame &inFrame, OptionalErrorCode ec)
{
    clear_if(ec);

    int gotPacket = 0;
    Packet packet;
    auto st = encodeCommon(packet, inFrame, gotPacket, avcodec_encode_video_legacy);

    if (get<1>(st)) {
        throws_if(ec, get<0>(st), *get<1>(st));
        return Packet();
    }

    if (!gotPacket) {
        packet.setComplete(false);
        return packet;
    }

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 60, 100)
    packet.setKeyPacket(!!m_raw->coded_frame->key_frame);
#endif

    packet.setComplete(true);
    return packet;
}

AudioSamples CodecContextDeprecated::decodeAudio(const Packet &inPacket, OptionalErrorCode ec)
{
    return decodeAudio(inPacket, 0, ec);
}

AudioSamples CodecContextDeprecated::decodeAudio(const Packet &inPacket, size_t offset, OptionalErrorCode ec)
{
    clear_if(ec);

    AudioSamples outSamples;

    int gotFrame = 0;
    auto st = decodeCommon(outSamples, inPacket, offset, gotFrame, avcodec_decode_audio_legacy);
    if (get<1>(st))
    {
        throws_if(ec, get<0>(st), *get<1>(st));
        return AudioSamples();
    }

    if (!gotFrame)
    {
        outSamples.setComplete(false);
    }

    // Fix channels layout
    if (outSamples.channelsCount() && !outSamples.channelsLayout())
        av_frame_set_channel_layout(outSamples.raw(), av_get_default_channel_layout(outSamples.channelsCount()));

    return outSamples;

}

Packet CodecContextDeprecated::encodeAudio(OptionalErrorCode ec)
{
    return encodeAudio(AudioSamples(nullptr), ec);
}

Packet CodecContextDeprecated::encodeAudio(const AudioSamples &inSamples, OptionalErrorCode ec)
{
    clear_if(ec);

    Packet outPacket;

    int gotFrame = 0;
    auto st = encodeCommon(outPacket, inSamples, gotFrame, avcodec_encode_audio_legacy);
    if (get<1>(st))
    {
        throws_if(ec, get<0>(st), *get<1>(st));
        return Packet();
    }

    if (!gotFrame)
    {
        outPacket.setComplete(false);
        return outPacket;
    }

    return outPacket;
}

bool CodecContextDeprecated::isValidForEncode()
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

    if (!isOpened())
    {
        fflog(AV_LOG_WARNING, "You must open coder before encoding\n");
        return false;
    }

    if (m_direction == Direction::Decoding)
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

std::pair<ssize_t, const error_category *> CodecContextDeprecated::decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    if (!isValid())
        return make_error_pair(Errors::CodecInvalid);

    if (!isOpened())
        return make_error_pair(Errors::CodecNotOpened);

    if (!decodeProc)
        return make_error_pair(Errors::CodecInvalidDecodeProc);

    if (offset && inPacket.size() && offset >= inPacket.size())
        return make_error_pair(Errors::CodecDecodingOffsetToLarge);

    frameFinished = 0;

    AVPacket pkt = *inPacket.raw();
    pkt.data += offset;
    pkt.size -= offset;

//#warning Inspect and remove
//    m_raw->reordered_opaque = inPacket.pts().timestamp();

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

std::pair<ssize_t, const error_category *> CodecContextDeprecated::encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
{
    if (!isValid()) {
        fflog(AV_LOG_ERROR, "Invalid context\n");
        return make_error_pair(Errors::CodecInvalid);
    }

    if (!isValidForEncode()) {
        fflog(AV_LOG_ERROR, "Context can't be used for encoding\n");
        return make_error_pair(Errors::CodecInvalidForEncode);
    }

    if (!encodeProc) {
        fflog(AV_LOG_ERROR, "Encoding proc is null\n");
        return make_error_pair(Errors::CodecInvalidEncodeProc);
    }

    int stat = encodeProc(m_raw, outPacket.raw(), inFrame, &gotPacket);
    if (stat) {
        fflog(AV_LOG_ERROR, "Encode error: %d, %s\n", stat, error2string(stat).c_str());
    }
    return make_error_pair(stat);;
}

template<typename T>
std::pair<ssize_t, const error_category *> CodecContextDeprecated::decodeCommon(T &outFrame, const Packet & inPacket, size_t offset, int & frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
{
    auto st = decodeCommon(outFrame.raw(), inPacket, offset, frameFinished, decodeProc);
    if (get<1>(st))
        return st;

    if (!frameFinished)
        return make_pair(0u, nullptr);

    // Dial with PTS/DTS in packet/stream timebase

    if (inPacket.timeBase() != Rational())
        outFrame.setTimeBase(inPacket.timeBase());
    else
        outFrame.setTimeBase(m_stream.timeBase());

    AVFrame *frame = outFrame.raw();

    if (frame->pts == AV_NOPTS_VALUE)
        frame->pts = av_frame_get_best_effort_timestamp(frame);

#if !defined(FF_API_PKT_PTS)
    if (frame->pts == AV_NOPTS_VALUE)
        frame->pts = frame->pkt_pts;
#endif

    if (frame->pts == AV_NOPTS_VALUE)
        frame->pts = frame->pkt_dts;

    // Convert to decoder/frame time base. Seems not nessesary.
    outFrame.setTimeBase(timeBase());

    if (inPacket)
        outFrame.setStreamIndex(inPacket.streamIndex());
    else
        outFrame.setStreamIndex(m_stream.index());

    outFrame.setComplete(true);

    return st;
}

template<typename T>
std::pair<ssize_t, const error_category *> CodecContextDeprecated::encodeCommon(Packet & outPacket, const T & inFrame, int & gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
{
    auto st = encodeCommon(outPacket, inFrame.raw(), gotPacket, encodeProc);
    if (get<1>(st))
        return st;
    if (!gotPacket)
        return make_pair(0u, nullptr);

    if (inFrame && inFrame.timeBase() != Rational()) {
        outPacket.setTimeBase(inFrame.timeBase());
        outPacket.setStreamIndex(inFrame.streamIndex());
    } else if (m_stream.isValid()) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 51, 100)
            outPacket.setTimeBase(av_stream_get_codec_timebase(m_stream.raw()));
#else
            if (m_stream.raw()->codec) {
                outPacket.setTimeBase(m_stream.raw()->codec->time_base);
            }
#endif
        outPacket.setStreamIndex(m_stream.index());
    }

    //clog << "-------- pts: " << outPacket.raw()->pts << ", dts: " << outPacket.raw()->dts << '\n';

    // Recalc PTS/DTS/Duration
    if (m_stream.isValid()) {
        outPacket.setTimeBase(m_stream.timeBase());
    }

    outPacket.setComplete(true);

    return st;
}

} // ::av
