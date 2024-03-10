#include <stdexcept>

#include "avlog.h"
#include "avutils.h"
#include "averror.h"

#include "stream.h"
#include "frame.h"
#include "packet.h"
#include "dictionary.h"
#include "codec.h"
#include "codecparameters.h"

#include "codeccontext.h"

using namespace std;

namespace av {

GenericCodecContext::GenericCodecContext(Stream st)
    : CodecContext2(st, Codec(), st.direction(), st.mediaType())
{
}

GenericCodecContext::GenericCodecContext(GenericCodecContext &&other)
    : GenericCodecContext()
{
    swap(other);
}

GenericCodecContext &GenericCodecContext::operator=(GenericCodecContext &&rhs)
{
    if (this == &rhs)
        return *this;
    GenericCodecContext(std::move(rhs)).swap(*this);
    return *this;
}

AVMediaType GenericCodecContext::codecType() const noexcept
{
    return codecType(stream().mediaType());
}

// ::anonymous
} // ::av

namespace av {


VideoDecoderContext::VideoDecoderContext(VideoDecoderContext &&other)
    : Parent(std::move(other))
{
}

VideoDecoderContext &VideoDecoderContext::operator=(VideoDecoderContext&& other)
{
    return moveOperator(std::move(other));
}

VideoEncoderContext::VideoEncoderContext(VideoEncoderContext &&other)
    : Parent(std::move(other))
{
}

VideoEncoderContext &VideoEncoderContext::operator=(VideoEncoderContext&& other)
{
    return moveOperator(std::move(other));
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
        st.codecParameters().copyTo(*this);
    }
#endif

    setTimeBase(st.timeBase());
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

#if LIBAVCODEC_VERSION_MAJOR < 58 // FFmpeg 4.0
    std::error_code ec;
    close(ec);
#endif
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
        avcodec_free_context(&m_raw);
        m_raw = avcodec_alloc_context3(codec.raw());
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
    // TBD: need a check
    if (m_stream.isValid()) {
        m_stream.codecParameters().copyFrom(*this);
    }
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
    CodecParameters params;
    params.copyFrom(other);
    params.copyTo(*this);
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
        if (sts) {
            throws_if(ec, sts, ffmpeg_category());
        }
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

int64_t CodecContext2::frameNumber() const noexcept
{
#if API_FRAME_NUM
    return RAW_GET2(isValid(), frame_num, 0);
#else
    return RAW_GET2(isValid(), frame_number, 0);
#endif
}

bool CodecContext2::isRefCountedFrames() const noexcept
{
    if (!isValid())
        return false;
    int64_t val;
    av_opt_get_int(m_raw, "refcounted_frames", 0, &val);
    return !!val;
}

void CodecContext2::setRefCountedFrames(bool refcounted) const noexcept
{
    if (isValid() && !isOpened()) {
        av_opt_set_int(m_raw, "refcounted_frames", refcounted, 0);
    }
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

int64_t CodecContext2::bitRate() const noexcept
{
    return RAW_GET2(isValid(), bit_rate, int64_t(0));
}

std::pair<int64_t, int64_t> CodecContext2::bitRateRange() const noexcept
{
    if (isValid())
        return std::make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
    else
        return std::make_pair(0, 0);
}

void CodecContext2::setBitRate(int64_t bitRate) noexcept
{
    RAW_SET2(isValid(), bit_rate, bitRate);
}

void CodecContext2::setBitRateRange(const std::pair<int64_t, int64_t> &bitRateRange) noexcept
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

AudioDecoderContext::AudioDecoderContext(AudioDecoderContext &&other)
    : Parent(std::move(other))
{
}

AudioDecoderContext &AudioDecoderContext::operator=(AudioDecoderContext &&other)
{
    return moveOperator(std::move(other));
}

AudioEncoderContext::AudioEncoderContext(AudioEncoderContext &&other)
    : Parent(std::move(other))
{
}

AudioEncoderContext &AudioEncoderContext::operator=(AudioEncoderContext &&other)
{
    return moveOperator(std::move(other));
}



error_code CodecContext2::checkDecodeEncodePreconditions() const noexcept
{
    if (!isValid())
        return make_error_code(Errors::CodecInvalid);

    if (!isOpened())
        return make_error_code(Errors::CodecNotOpened);

    // if (!decodeProc)
    //     return make_error_code(Errors::CodecInvalidDecodeProc);
    // if (offset && inPacket.size() && offset >= inPacket.size())
    //     return make_error_code(Errors::CodecDecodingOffsetToLarge);
    return {};
}


#undef warnIfNotAudio
#undef warnIfNotVideo

namespace codec_context::audio {

//
// TBD: make a generic function
//
void set_channels(AVCodecContext *obj, int channels)
{
#if API_NEW_CHANNEL_LAYOUT
    if (!av_channel_layout_check(&obj->ch_layout) || (obj->ch_layout.nb_channels != channels)) {
        av_channel_layout_uninit(&obj->ch_layout);
        av_channel_layout_default(&obj->ch_layout, channels);
    }
#else
    obj->channels = channels;
    if (obj->channel_layout != 0 || av_get_channel_layout_nb_channels(obj->channel_layout) != channels) {
        obj->channel_layout = av_get_default_channel_layout(channels);
    }
#endif
}

void set_channel_layout_mask(AVCodecContext *obj, uint64_t mask)
{
#if API_NEW_CHANNEL_LAYOUT
    if (!av_channel_layout_check(&obj->ch_layout) ||
        (obj->ch_layout.order != AV_CHANNEL_ORDER_NATIVE) ||
        ((obj->ch_layout.order == AV_CHANNEL_ORDER_NATIVE) && (obj->ch_layout.u.mask != mask))) {
        av_channel_layout_uninit(&obj->ch_layout);
        av_channel_layout_from_mask(&obj->ch_layout, mask);
    }
#else
    obj->channel_layout = mask;

    // Make channels and channel_layout sync
    if (obj->channels == 0 ||
        (uint64_t)av_get_default_channel_layout(obj->channels) != mask)
    {
        obj->channels = av_get_channel_layout_nb_channels(mask);
    }
#endif
}

int get_channels(const AVCodecContext *obj)
{
#if API_NEW_CHANNEL_LAYOUT
    return obj->ch_layout.nb_channels;
#else
    if (obj->channels)
        return obj->channels;

    if (obj->channel_layout)
        return av_get_channel_layout_nb_channels(obj->channel_layout);

    return 0;
#endif
}


uint64_t get_channel_layout_mask(const AVCodecContext *obj)
{
#if API_NEW_CHANNEL_LAYOUT
    return obj->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ? obj->ch_layout.u.mask : 0;
#else
    if (obj->channel_layout)
        return obj->channel_layout;

    if (obj->channels)
        return av_get_default_channel_layout(obj->channels);

    return 0;
#endif
}

}

} // namespace av
