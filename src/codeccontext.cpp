#include <stdexcept>

#include "avlog.h"
#include "avutils.h"
#include "averror.h"

#include "codeccontext.h"

using namespace std;

namespace av {
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

} // ::anonymous
} // ::av

namespace {

#define NEW_CODEC_API (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,37,100))

#if NEW_CODEC_API
// Use avcodec_send_packet() and avcodec_receive_frame()
int decode(AVCodecContext *avctx,
           AVFrame *picture,
           int *got_picture_ptr,
           const AVPacket *avpkt)
{
    if (got_picture_ptr)
        *got_picture_ptr = 0;

    int ret;
    if (avpkt) {
        ret = avcodec_send_packet(avctx, avpkt);
        if (ret < 0 && ret != AVERROR_EOF)
            return ret;
    }

    ret = avcodec_receive_frame(avctx, picture);
    if (ret < 0 && ret != AVERROR(EAGAIN))
        return ret;
    if (ret >= 0 && got_picture_ptr)
        *got_picture_ptr = 1;

    return 0;
}

// Use avcodec_send_frame() and avcodec_receive_packet()
int encode(AVCodecContext *avctx,
           AVPacket *avpkt,
           const AVFrame *frame,
           int *got_packet_ptr)
{
    if (got_packet_ptr)
        *got_packet_ptr = 0;

    int ret;
    if (frame) {
        ret = avcodec_send_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return ret;
    }

    ret = avcodec_receive_packet(avctx, avpkt);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return ret;
    if (got_packet_ptr)
        *got_packet_ptr = 1;
    return 0;
}

int avcodec_decode_video_legacy(AVCodecContext *avctx, AVFrame *picture,
                                int *got_picture_ptr,
                                const AVPacket *avpkt)
{
    return decode(avctx, picture, got_picture_ptr, avpkt);
}

int avcodec_encode_video_legacy(AVCodecContext *avctx, AVPacket *avpkt,
                                const AVFrame *frame, int *got_packet_ptr)
{
    return encode(avctx, avpkt, frame, got_packet_ptr);
}

int avcodec_decode_audio_legacy(AVCodecContext *avctx, AVFrame *frame,
                                int *got_frame_ptr, const AVPacket *avpkt)
{
    return decode(avctx, frame, got_frame_ptr, avpkt);
}

int avcodec_encode_audio_legacy(AVCodecContext *avctx, AVPacket *avpkt,
                          const AVFrame *frame, int *got_packet_ptr)
{
    return encode(avctx, avpkt, frame, got_packet_ptr);
}
#else
int avcodec_decode_video_legacy(AVCodecContext *avctx, AVFrame *picture,
                                int *got_picture_ptr,
                                const AVPacket *avpkt)
{
    return avcodec_decode_video2(avctx, picture, got_picture_ptr, avpkt);
}

int avcodec_encode_video_legacy(AVCodecContext *avctx, AVPacket *avpkt,
                                const AVFrame *frame, int *got_packet_ptr)
{
    return avcodec_encode_video2(avctx, avpkt, frame, got_packet_ptr);
}

int avcodec_decode_audio_legacy(AVCodecContext *avctx, AVFrame *frame,
                                int *got_frame_ptr, const AVPacket *avpkt)
{
    return avcodec_decode_audio4(avctx, frame, got_frame_ptr, avpkt);
}

int avcodec_encode_audio_legacy(AVCodecContext *avctx, AVPacket *avpkt,
                          const AVFrame *frame, int *got_packet_ptr)
{
    return avcodec_encode_audio2(avctx, avpkt, frame, got_packet_ptr);
}
#endif

} //::anonymous

#include "codeccontext_deprecated.inl"

namespace av {


VideoDecoderContext::VideoDecoderContext(VideoDecoderContext &&other)
    : Parent(std::move(other))
{
}

VideoDecoderContext &VideoDecoderContext::operator=(VideoDecoderContext&& other)
{
    return moveOperator(std::move(other));
}

VideoFrame VideoDecoderContext::decode(const Packet &packet, OptionalErrorCode ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, 0, nullptr, autoAllocateFrame);
}

VideoFrame VideoDecoderContext::decode(const Packet &packet, size_t offset, size_t &decodedBytes, OptionalErrorCode ec, bool autoAllocateFrame)
{
    return decodeVideo(ec, packet, offset, &decodedBytes, autoAllocateFrame);
}

VideoFrame VideoDecoderContext::decodeVideo(OptionalErrorCode ec, const Packet &packet, size_t offset, size_t *decodedBytes, bool autoAllocateFrame)
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

VideoEncoderContext::VideoEncoderContext(VideoEncoderContext &&other)
    : Parent(std::move(other))
{
}

VideoEncoderContext &VideoEncoderContext::operator=(VideoEncoderContext&& other)
{
    return moveOperator(std::move(other));
}

Packet VideoEncoderContext::encode(OptionalErrorCode ec)
{
    return encode(VideoFrame(nullptr), ec);
}

Packet VideoEncoderContext::encode(const VideoFrame &inFrame, OptionalErrorCode ec)
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

#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    auto const codecId = st.raw()->codec->codec_id;
    FF_ENABLE_DEPRECATION_WARNINGS
#else
    auto const codecId = st.raw()->codecpar->codec_id;
#endif

    Codec c = codec;
    if (codec.isNull())
    {
        if (st.direction() == Direction::Decoding)
            c = findDecodingCodec(codecId);
        else
            c = findEncodingCodec(codecId);
    }

#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    m_raw = st.raw()->codec;
    if (!c.isNull())
        setCodec(c, false, direction, type);
    FF_ENABLE_DEPRECATION_WARNINGS
#else
    m_raw = avcodec_alloc_context3(c.raw());
    if (m_raw) {
        avcodec_parameters_to_context(m_raw, st.raw()->codecpar);
    }
#endif
}

CodecContext2::CodecContext2(const Codec &codec, Direction direction, AVMediaType type)
{
    if (checkCodec(codec, direction, type, throws()))
        m_raw = avcodec_alloc_context3(codec.raw());
}

CodecContext2::~CodecContext2()
{
    //
    // Do not track stream-oriented codec:
    //  - Stream always owned by FormatContext
    //  - Stream always can be obtained from the FormatContext
    //  - CodecContext can be obtained at any time from the Stream
    //  - If FormatContext closed, all streams destroyed: all opened codec context closed too.
    // So only stream-independ CodecContext's must be tracked and closed in ctor.
    // Note: new FFmpeg API declares, that AVStream not owned by codec and deals only with codecpar
    //
#if !USE_CODECPAR
    if (!m_stream.isNull())
        return;
#endif

    std::error_code ec;
    close(ec);
    avcodec_free_context(&m_raw);
}

void CodecContext2::setCodec(const Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, OptionalErrorCode ec)
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

#if !USE_CODECPAR // AVFORMAT < 57.5.0
    FF_DISABLE_DEPRECATION_WARNINGS
    if (m_stream.isValid()) {
        m_stream.raw()->codec = m_raw;
    }
    FF_ENABLE_DEPRECATION_WARNINGS
#else
    //if (m_stream.isValid())
    //    avcodec_parameters_from_context(m_stream.raw()->codecpar, m_raw);
#endif
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

void CodecContext2::open(OptionalErrorCode ec)
{
    open(Codec(), ec);
}

void CodecContext2::open(const Codec &codec, OptionalErrorCode ec)
{
    open(codec, nullptr, ec);
}

void CodecContext2::open(Dictionary &&options, OptionalErrorCode ec)
{
    open(std::move(options), Codec(), ec);
}

void CodecContext2::open(Dictionary &options, OptionalErrorCode ec)
{
    open(options, Codec(), ec);
}

void CodecContext2::open(Dictionary &&options, const Codec &codec, OptionalErrorCode ec)
{
    open(options, codec, ec);
}

void CodecContext2::open(Dictionary &options, const Codec &codec, OptionalErrorCode ec)
{
    auto prt = options.release();
    open(codec, &prt, ec);
    options.assign(prt);
}

void CodecContext2::close(OptionalErrorCode ec)
{
    clear_if(ec);
    if (isOpened())
    {
        avcodec_close(m_raw);
        return;
    }
    throws_if(ec, Errors::CodecNotOpened);
}

bool CodecContext2::isOpened() const noexcept
{
    return isValid() ? avcodec_is_open(m_raw) : false;
}

bool CodecContext2::isValid() const noexcept
{
    // Check parent stream first
    return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
}

void CodecContext2::copyContextFrom(const CodecContext2 &other, OptionalErrorCode ec)
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

#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    int stat = avcodec_copy_context(m_raw, other.m_raw);
    if (stat < 0)
        throws_if(ec, stat, ffmpeg_category());
    FF_ENABLE_DEPRECATION_WARNINGS
#else
    AVCodecParameters params{};
    avcodec_parameters_from_context(&params, other.m_raw);
    avcodec_parameters_to_context(m_raw, &params);
#endif
    m_raw->codec_tag = 0;
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

void CodecContext2::setOption(const string &key, const string &val, OptionalErrorCode ec)
{
    setOption(key, val, 0, ec);
}

void CodecContext2::setOption(const string &key, const string &val, int flags, OptionalErrorCode ec)
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

bool CodecContext2::isValidForEncode(Direction direction, AVMediaType /*type*/) const noexcept
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

bool CodecContext2::checkCodec(const Codec &codec, Direction direction, AVMediaType type, OptionalErrorCode ec)
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

void CodecContext2::open(const Codec &codec, AVDictionary **options, OptionalErrorCode ec)
{
    clear_if(ec);

    if (isOpened() || !isValid()) {
        throws_if(ec, isOpened() ? Errors::CodecAlreadyOpened : Errors::CodecInvalid);
        return;
    }

    int stat = avcodec_open2(m_raw, codec.raw(), options);
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

AudioSamples AudioDecoderContext::decode(const Packet &inPacket, OptionalErrorCode ec)
{
    return decode(inPacket, 0u, ec);
}

AudioSamples AudioDecoderContext::decode(const Packet &inPacket, size_t offset, OptionalErrorCode ec)
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

AudioEncoderContext::AudioEncoderContext(AudioEncoderContext &&other)
    : Parent(move(other))
{
}

AudioEncoderContext &AudioEncoderContext::operator=(AudioEncoderContext &&other)
{
    return moveOperator(move(other));
}

Packet AudioEncoderContext::encode(OptionalErrorCode ec)
{
    return encode(AudioSamples(nullptr), ec);
}

Packet AudioEncoderContext::encode(const AudioSamples &inSamples, OptionalErrorCode ec)
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


#undef warnIfNotAudio
#undef warnIfNotVideo

} // namespace av
