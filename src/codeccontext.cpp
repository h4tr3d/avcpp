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

CodecContext::CodecContext(const Stream &st, const Codec &codec)
    : m_stream(st)
{
    m_raw = st.raw()->codec;

    Codec c = codec;

    if (codec.isNull()) {
        if (st.direction() == Direction::Decoding)
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
    swap(m_stream, other.m_stream);
    swap(m_raw, other.m_raw);
}

void CodecContext::setCodec(const Codec &codec, bool resetDefaults, error_code &ec)
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

    if (isOpened() || !isValid()) {
        throws_if(ec, isOpened() ? Errors::CodecAlreadyOpened : Errors::CodecInvalid);
        return;
    }

    int stat = avcodec_open2(m_raw, codec.isNull() ? m_raw->codec : codec.raw(), options);
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
}

void CodecContext::close(error_code &ec)
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

bool CodecContext::isOpened() const noexcept
{
    return m_raw ? avcodec_is_open(m_raw) : false;
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

    int stat = avcodec_copy_context(m_raw, other.m_raw);
    m_raw->codec_tag = 0;
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
}

Rational CodecContext::timeBase() const
{
    return RAW_GET2(isValid(), time_base, AVRational{});
}

void CodecContext::setTimeBase(const Rational &value)
{
    RAW_SET2(isValid(), time_base, value.getValue());
}

const Stream &CodecContext::stream() const
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
        throws_if(ec, Errors::CodecInvalid);
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

bool CodecContext::isRefCountedFrames() const noexcept
{
    return RAW_GET2(isValid(), refcounted_frames, false);
}

void CodecContext::setRefCountedFrames(bool refcounted) const noexcept
{
    RAW_SET2(isValid() && !isOpened(), refcounted_frames, refcounted);
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

PixelFormat CodecContext::pixelFormat() const
{
    return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
}

int32_t CodecContext::bitRate() const
{
    return RAW_GET2(isValid(), bit_rate, int32_t(0));
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
    if (isValid())
    {
        m_raw->width       = w;
        m_raw->coded_width = w;
    }
}

void CodecContext::setHeight(int h)
{
    warnIfNotVideo();
    if (isValid())
    {
        m_raw->height       = h;
        m_raw->coded_height = h;
    }
}

void CodecContext::setCodedWidth(int w)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), coded_width, w);
}

void CodecContext::setCodedHeight(int h)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), coded_height, h);
}

void CodecContext::setPixelFormat(PixelFormat pixelFormat)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), pix_fmt, pixelFormat);
}

void CodecContext::setBitRate(int32_t bitRate)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), bit_rate, bitRate);
}

void CodecContext::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    warnIfNotVideo();
    if (isValid())
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

    RAW_SET2(isValid(), global_quality, quality);
}

void CodecContext::setGopSize(int32_t size)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), gop_size, size);
}

void CodecContext::setBitRateTolerance(int bitRateTolerance)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), bit_rate_tolerance, bitRateTolerance);
}

void CodecContext::setStrict(int strict)
{
    warnIfNotVideo();
    if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        strict = FF_COMPLIANCE_EXPERIMENTAL;
    else if (strict > FF_COMPLIANCE_VERY_STRICT)
        strict = FF_COMPLIANCE_VERY_STRICT;

    RAW_SET2(isValid(), strict_std_compliance, strict);
}

void CodecContext::setMaxBFrames(int maxBFrames)
{
    warnIfNotVideo();
    RAW_SET2(isValid(), max_b_frames, maxBFrames);
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

SampleFormat CodecContext::sampleFormat() const
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

void CodecContext::setChannels(int channels)
{
    if (!isValid() || channels <= 0)
        return;
    m_raw->channels = channels;
    if (m_raw->channel_layout != 0 ||
        av_get_channel_layout_nb_channels(m_raw->channel_layout) != channels) {
        m_raw->channel_layout = av_get_default_channel_layout(channels);
    }
}

void CodecContext::setSampleFormat(SampleFormat sampleFormat)
{
    warnIfNotAudio();
    RAW_SET2(isValid(), sample_fmt, sampleFormat);
}

void CodecContext::setChannelLayout(uint64_t layout)
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

void CodecContext::setFlags(int flags)
{
    RAW_SET2(isValid(), flags, flags);
}

void CodecContext::addFlags(int flags)
{
    if (isValid())
        m_raw->flags |= flags;
}

void CodecContext::clearFlags(int flags)
{
    if (isValid())
        m_raw->flags &= ~flags;
}

int CodecContext::flags()
{
    return RAW_GET2(isValid(), flags, 0);
}

bool CodecContext::isFlags(int flags)
{
    if (isValid())
        return (m_raw->flags & flags);
    return false;
}

//
void CodecContext::setFlags2(int flags)
{
    RAW_SET2(isValid(), flags2, flags);
}

void CodecContext::addFlags2(int flags)
{
    if (isValid())
        m_raw->flags2 |= flags;
}

void CodecContext::clearFlags2(int flags)
{
    if (isValid())
        m_raw->flags2 &= ~flags;
}

int CodecContext::flags2()
{
    return RAW_GET2(isValid(), flags2, 0);
}

bool CodecContext::isFlags2(int flags)
{
    if (isValid())
        return (m_raw->flags2 & flags);
    return false;
}

VideoFrame CodecContext::decodeVideo(const Packet &packet, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, 0, nullptr, autoAllocateFrame);
}

VideoFrame CodecContext::decodeVideo(const Packet &packet, size_t offset, size_t &decodedBytes, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, offset, &decodedBytes, autoAllocateFrame);
}

Packet CodecContext::encodeVideo(error_code &ec)
{
    return encodeVideo(VideoFrame(nullptr), ec);
}

VideoFrame CodecContext::decodeVideo(error_code &ec, const Packet &packet, size_t offset, size_t *decodedBytes, bool autoAllocateFrame)
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
    auto st = decodeCommon(outFrame, packet, offset, gotFrame, avcodec_decode_video2);

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


Packet CodecContext::encodeVideo(const VideoFrame &inFrame, error_code &ec)
{
    clear_if(ec);

    int gotPacket = 0;
    Packet packet;
    auto st = encodeCommon(packet, inFrame, gotPacket, avcodec_encode_video2);

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

AudioSamples CodecContext::decodeAudio(const Packet &inPacket, error_code &ec)
{
    return decodeAudio(inPacket, 0, ec);
}

AudioSamples CodecContext::decodeAudio(const Packet &inPacket, size_t offset, error_code &ec)
{
    clear_if(ec);

    AudioSamples outSamples;

    int gotFrame = 0;
    auto st = decodeCommon(outSamples, inPacket, offset, gotFrame, avcodec_decode_audio4);
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

Packet CodecContext::encodeAudio(error_code &ec)
{
    return encodeAudio(AudioSamples(nullptr), ec);
}

Packet CodecContext::encodeAudio(const AudioSamples &inSamples, error_code &ec)
{
    clear_if(ec);

    Packet outPacket;

    int gotFrame = 0;
    auto st = encodeCommon(outPacket, inSamples, gotFrame, avcodec_encode_audio2);
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

namespace {

std::pair<ssize_t, const error_category*>
make_error_pair(Errors errc)
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

std::pair<ssize_t, const error_category *> CodecContext::encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
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
std::pair<ssize_t, const error_category *> CodecContext::decodeCommon(T &outFrame, const Packet & inPacket, size_t offset, int & frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
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

    if (frame->pts == AV_NOPTS_VALUE)
        frame->pts = frame->pkt_pts;

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
std::pair<ssize_t, const error_category *> CodecContext::encodeCommon(Packet & outPacket, const T & inFrame, int & gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
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
        if (m_stream.raw()->codec) {
            outPacket.setTimeBase(m_stream.raw()->codec->time_base);
        }
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

VideoDecoderContext::VideoDecoderContext(VideoDecoderContext &&other)
    : Parent(std::move(other))
{
}

VideoDecoderContext &VideoDecoderContext::operator=(VideoDecoderContext&& other)
{
    return moveOperator(std::move(other));
}

VideoFrame VideoDecoderContext::decode(const Packet &packet, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, 0, nullptr, autoAllocateFrame);
}

VideoFrame VideoDecoderContext::decode(const Packet &packet, size_t offset, size_t &decodedBytes, error_code &ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, offset, &decodedBytes, autoAllocateFrame);
}

VideoFrame VideoDecoderContext::decodeVideo(error_code &ec, const Packet &packet, size_t offset, size_t *decodedBytes, bool autoAllocateFrame)
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
    auto st = decodeCommon(outFrame, packet, offset, gotFrame, avcodec_decode_video2);

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

VideoEncoderContext::VideoEncoderContext(VideoEncoderContext &&other)
    : Parent(std::move(other))
{
}

VideoEncoderContext &VideoEncoderContext::operator=(VideoEncoderContext&& other)
{
    return moveOperator(std::move(other));
}

Packet VideoEncoderContext::encode(error_code &ec)
{
    return encode(VideoFrame(nullptr), ec);
}

Packet VideoEncoderContext::encode(const VideoFrame &inFrame, error_code &ec)
{
    clear_if(ec);

    int gotPacket = 0;
    Packet packet;
    auto st = encodeCommon(packet, inFrame, gotPacket, avcodec_encode_video2);

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

void CodecContext2::swap(CodecContext2 &other)
{
    using std::swap;
    swap(m_stream, other.m_stream);
    swap(m_raw, other.m_raw);
}

CodecContext2::CodecContext2()
{
    m_raw = avcodec_alloc_context3(nullptr);
}

CodecContext2::CodecContext2(const Stream &st, const Codec &codec, Direction direction, AVMediaType type)
    : m_stream(st)
{
    if (st.direction() != direction)
        throw av::Exception(make_avcpp_error(Errors::CodecInvalidDirection));

    if (st.mediaType() != type)
        throw av::Exception(make_avcpp_error(Errors::CodecInvalidMediaType));

    m_raw = st.raw()->codec;

    Codec c = codec;

    if (codec.isNull())
    {
        if (st.direction() == Direction::Decoding)
            c = findDecodingCodec(m_raw->codec_id);
        else
            c = findEncodingCodec(m_raw->codec_id);
    }

    if (!c.isNull())
        setCodec(c, false, direction, type);
}

CodecContext2::CodecContext2(const Codec &codec, Direction direction, AVMediaType type)
{
    if (checkCodec(codec, direction, type, throws()))
        m_raw = avcodec_alloc_context3(codec.raw());
}

CodecContext2::~CodecContext2()
{
    std::error_code ec;
    close(ec);
    if (m_stream.isNull())
        av_freep(&m_raw);
}

void CodecContext2::setCodec(const Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, error_code &ec)
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

    if (!checkCodec(codec, direction, type, ec))
        return;

    if (resetDefaults) {
        if (!m_raw->codec) {
            avcodec_free_context(&m_raw);
            m_raw = avcodec_alloc_context3(codec.raw());
        } else {
            avcodec_get_context_defaults3(m_raw, codec.raw());
        }
    } else {
        m_raw->codec_id   = !codec.isNull() ? codec.raw()->id : AV_CODEC_ID_NONE;
        m_raw->codec_type = type;
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

AVMediaType CodecContext2::codecType(AVMediaType contextType) const noexcept
{
    if (isValid())
    {
        if (m_raw->codec && (m_raw->codec_type != m_raw->codec->type || m_raw->codec_type != contextType))
            fflog(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type and/or context type\n");

        return m_raw->codec_type;
    }
    return contextType;
}

void CodecContext2::open(error_code &ec)
{
    open(Codec(), ec);
}

void CodecContext2::open(const Codec &codec, error_code &ec)
{
    open(codec, nullptr, ec);
}

void CodecContext2::open(Dictionary &&options, error_code &ec)
{
    open(std::move(options), Codec(), ec);
}

void CodecContext2::open(Dictionary &options, error_code &ec)
{
    open(options, Codec(), ec);
}

void CodecContext2::open(Dictionary &&options, const Codec &codec, error_code &ec)
{
    open(options, codec, ec);
}

void CodecContext2::open(Dictionary &options, const Codec &codec, error_code &ec)
{
    auto prt = options.release();
    open(codec, &prt, ec);
    options.assign(prt);
}

void CodecContext2::close(error_code &ec)
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

bool CodecContext2::isOpened() const noexcept
{
    return m_raw ? avcodec_is_open(m_raw) : false;
}

bool CodecContext2::isValid() const noexcept
{
    // Check parent stream first
    return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
}

void CodecContext2::copyContextFrom(const CodecContext2 &other, error_code &ec)
{
    clear_if(ec);
    if (!isValid()) {
        fflog(AV_LOG_ERROR, "Invalid target context\n");
        throws_if(ec, Errors::CodecInvalid);
        return;
    }
    if (!other.isValid()) {
        fflog(AV_LOG_ERROR, "Invalid source context\n");
        throws_if(ec, Errors::CodecInvalid);
        return;
    }
    if (isOpened()) {
        fflog(AV_LOG_ERROR, "Try to copy context to opened target context\n");
        throws_if(ec, Errors::CodecAlreadyOpened);
        return;
    }
    // TODO: need to be checked
    if (m_raw->codec_type != AVMEDIA_TYPE_UNKNOWN &&
        m_raw->codec_type != other.m_raw->codec_type)
    {
        fflog(AV_LOG_ERROR, "Context media types not same");
        throws_if(ec, Errors::CodecInvalidMediaType);
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

Rational CodecContext2::timeBase() const noexcept
{
    return RAW_GET2(isValid(), time_base, AVRational());
}

void CodecContext2::setTimeBase(const Rational &value) noexcept
{
    RAW_SET2(isValid() && !isOpened(), time_base, value.getValue());
}

const Stream &CodecContext2::stream() const noexcept
{
    return m_stream;
}

Codec CodecContext2::codec() const noexcept
{
    if (isValid())
        return Codec(m_raw->codec);
    else
        return Codec();
}

void CodecContext2::setOption(const string &key, const string &val, error_code &ec)
{
    setOption(key, val, 0, ec);
}

void CodecContext2::setOption(const string &key, const string &val, int flags, error_code &ec)
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

int CodecContext2::frameSize() const noexcept
{
    return RAW_GET2(isValid(), frame_size, 0);
}

int CodecContext2::frameNumber() const noexcept
{
    return RAW_GET2(isValid(), frame_number, 0);
}

bool CodecContext2::isRefCountedFrames() const noexcept
{
    return RAW_GET2(isValid(), refcounted_frames, false);
}

void CodecContext2::setRefCountedFrames(bool refcounted) const noexcept
{
    RAW_SET2(isValid() && !isOpened(), refcounted_frames, refcounted);
}

int CodecContext2::strict() const noexcept
{
    return RAW_GET2(isValid(), strict_std_compliance, 0);
}

void CodecContext2::setStrict(int strict) noexcept
{
    if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        strict = FF_COMPLIANCE_EXPERIMENTAL;
    else if (strict > FF_COMPLIANCE_VERY_STRICT)
        strict = FF_COMPLIANCE_VERY_STRICT;

    RAW_SET2(isValid(), strict_std_compliance, strict);
}

int32_t CodecContext2::bitRate() const noexcept
{
    return RAW_GET2(isValid(), bit_rate, int32_t(0));
}

std::pair<int, int> CodecContext2::bitRateRange() const noexcept
{
    if (isValid())
        return std::make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
    else
        return std::make_pair(0, 0);
}

void CodecContext2::setBitRate(int32_t bitRate) noexcept
{
    RAW_SET2(isValid(), bit_rate, bitRate);
}

void CodecContext2::setBitRateRange(const std::pair<int, int> &bitRateRange) noexcept
{
    if (isValid())
    {
        m_raw->rc_min_rate = std::get<0>(bitRateRange);
        m_raw->rc_max_rate = std::get<1>(bitRateRange);
    }
}

void CodecContext2::setFlags(int flags) noexcept
{
    RAW_SET2(isValid(), flags, flags);
}

void CodecContext2::addFlags(int flags) noexcept
{
    if (isValid())
        m_raw->flags |= flags;
}

void CodecContext2::clearFlags(int flags) noexcept
{
    if (isValid())
        m_raw->flags &= ~flags;
}

int CodecContext2::flags() noexcept
{
    return RAW_GET2(isValid(), flags, 0);
}

bool CodecContext2::isFlags(int flags) noexcept
{
    if (isValid())
        return (m_raw->flags & flags);
    return false;
}

void CodecContext2::setFlags2(int flags) noexcept
{
    RAW_SET2(isValid(), flags2, flags);
}

void CodecContext2::addFlags2(int flags) noexcept
{
    if (isValid())
        m_raw->flags2 |= flags;
}

void CodecContext2::clearFlags2(int flags) noexcept
{
    if (isValid())
        m_raw->flags2 &= ~flags;
}

int CodecContext2::flags2() noexcept
{
    return RAW_GET2(isValid(), flags2, 0);
}

bool CodecContext2::isFlags2(int flags) noexcept
{
    if (isValid())
        return (m_raw->flags2 & flags);
    return false;
}

bool CodecContext2::isValidForEncode(Direction direction, AVMediaType type) const noexcept
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

    if (direction == Direction::Decoding)
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

bool CodecContext2::checkCodec(const Codec &codec, Direction direction, AVMediaType type, error_code &ec)
{
    if (direction == Direction::Encoding && !codec.canEncode())
    {
        fflog(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
        throws_if(ec, Errors::CodecInvalidDirection);
        return false;
    }

    if (direction == Direction::Decoding && !codec.canDecode())
    {
        fflog(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
        throws_if(ec, Errors::CodecInvalidDirection);
        return false;
    }

    if (type != codec.type())
    {
        fflog(AV_LOG_ERROR, "Media type mismatch\n");
        throws_if(ec, Errors::CodecInvalidMediaType);
        return false;
    }

    return true;
}

void CodecContext2::open(const Codec &codec, AVDictionary **options, error_code &ec)
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

std::pair<ssize_t, const error_category *> CodecContext2::decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished, int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *)) noexcept
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

    int decoded = decodeProc(m_raw, outFrame, &frameFinished, &pkt);
    return make_error_pair(decoded);
}

std::pair<ssize_t, const error_category *> CodecContext2::encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket, int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *)) noexcept
{
    if (!isValid()) {
        fflog(AV_LOG_ERROR, "Invalid context\n");
        return make_error_pair(Errors::CodecInvalid);
    }

    //        if (!isValidForEncode()) {
    //            fflog(AV_LOG_ERROR, "Context can't be used for encoding\n");
    //            return make_error_pair(Errors::CodecInvalidForEncode);
    //        }

    if (!encodeProc) {
        fflog(AV_LOG_ERROR, "Encoding proc is null\n");
        return make_error_pair(Errors::CodecInvalidEncodeProc);
    }

    int stat = encodeProc(m_raw, outPacket.raw(), inFrame, &gotPacket);
    if (stat) {
        fflog(AV_LOG_ERROR, "Encode error: %d, %s\n", stat, error2string(stat).c_str());
    }
    return make_error_pair(stat);
}

AudioDecoderContext::AudioDecoderContext(AudioDecoderContext &&other)
    : Parent(std::move(other))
{
}

AudioDecoderContext &AudioDecoderContext::operator=(AudioDecoderContext &&other)
{
    return moveOperator(std::move(other));
}

AudioSamples AudioDecoderContext::decode(const Packet &inPacket, error_code &ec)
{
    return decode(inPacket, 0u, ec);
}

AudioSamples AudioDecoderContext::decode(const Packet &inPacket, size_t offset, error_code &ec)
{
    clear_if(ec);

    AudioSamples outSamples;

    int gotFrame = 0;
    auto st = decodeCommon(outSamples, inPacket, offset, gotFrame, avcodec_decode_audio4);
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

AudioEncoderContext::AudioEncoderContext(AudioEncoderContext &&other)
    : Parent(move(other))
{
}

AudioEncoderContext &AudioEncoderContext::operator=(AudioEncoderContext &&other)
{
    return moveOperator(move(other));
}

Packet AudioEncoderContext::encode(error_code &ec)
{
    return encode(AudioSamples(nullptr), ec);
}

Packet AudioEncoderContext::encode(const AudioSamples &inSamples, error_code &ec)
{
    clear_if(ec);

    Packet outPacket;

    int gotFrame = 0;
    auto st = encodeCommon(outPacket, inSamples, gotFrame, avcodec_encode_audio2);
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


#undef warnIfNotAudio
#undef warnIfNotVideo

} // namespace av
