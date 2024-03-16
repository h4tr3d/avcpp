#pragma once

#include "ffmpeg.h"
#include "stream.h"
#include "avutils.h"
#include "averror.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "avlog.h"
#include "frame.h"
#include "codec.h"
#include "channellayout.h"
#include "packet.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>
}

namespace av {

namespace detail {
#if USE_CONCEPTS
template<typename T>
concept valid_frame_type = std::is_same_v<T, VideoFrame> || std::is_same_v<T, AudioSamples>;
template<typename Ret, typename Fn, typename... Args>
concept invocable_r = std::is_invocable_r_v<Ret, Fn, Args...>;
#endif
}

/**
 * Return type for the Encoding/Deconding callbacks (result handlers)
 */
using dec_enc_handler_result_t = av::expected<bool, std::error_code>;

namespace codec_context::audio {
void set_channels(AVCodecContext *obj, int channels);
void set_channel_layout_mask(AVCodecContext *obj, uint64_t mask);
int get_channels(const AVCodecContext *obj);
uint64_t get_channel_layout_mask(const AVCodecContext *obj);
}

class CodecContext2 : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
protected:
    void swap(CodecContext2 &other);

    //
    // No directly created
    //

    using BaseWrapper = FFWrapperPtr<AVCodecContext>;
    using BaseWrapper::BaseWrapper;

    CodecContext2();

    // Stream decoding/encoding
    CodecContext2(const class Stream &st,
                  const class Codec& codec,
                  Direction direction,
                  AVMediaType type);

    // Stream independ decoding/encoding
    CodecContext2(const class Codec &codec, Direction direction, AVMediaType type);

    ~CodecContext2();

    void setCodec(const class Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, OptionalErrorCode ec = throws());

    AVMediaType codecType(AVMediaType contextType) const noexcept;

public:

    using BaseWrapper::_log;

    //
    // Common
    //

    void open(OptionalErrorCode ec = throws());
    void open(const Codec &codec, OptionalErrorCode ec = throws());
    void open(class Dictionary &options, OptionalErrorCode ec = throws());
    void open(class Dictionary &&options, OptionalErrorCode ec = throws());
    void open(class Dictionary &options, const Codec &codec, OptionalErrorCode ec = throws());
    void open(class Dictionary &&options, const Codec &codec, OptionalErrorCode ec = throws());

    [[deprecated("Start from FFmpeg 4.0 it is recommended to destroy and recreate codec context insted of close")]]
    void close(OptionalErrorCode ec = throws());

    bool isOpened() const noexcept;
    bool isValid() const noexcept;

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     */
    /// @{
    void copyContextFrom(const CodecContext2 &other, OptionalErrorCode ec = throws());
    /// @}

    Rational timeBase() const noexcept;
    void setTimeBase(const Rational &value) noexcept;

    const Stream& stream() const noexcept;
    Codec codec() const noexcept;

    void setOption(const std::string &key, const std::string &val, OptionalErrorCode ec = throws());
    void setOption(const std::string &key, const std::string &val, int flags, OptionalErrorCode ec = throws());

    int frameSize() const noexcept;
    int64_t frameNumber() const noexcept;

    // Note, set ref counted to enable for multithreaded processing
    bool isRefCountedFrames() const noexcept;
    void setRefCountedFrames(bool refcounted) const noexcept;

    int strict() const noexcept;
    void setStrict(int strict) noexcept;

    int64_t bitRate() const noexcept;
    std::pair<int64_t, int64_t> bitRateRange() const noexcept;
    void setBitRate(int64_t bitRate) noexcept;
    void setBitRateRange(const std::pair<int64_t, int64_t> &bitRateRange) noexcept;

    // Flags
    /// Access to CODEC_FLAG_* flags
    /// @{
    void setFlags(int flags) noexcept;
    void addFlags(int flags) noexcept;
    void clearFlags(int flags) noexcept;
    int flags() noexcept;
    bool isFlags(int flags) noexcept;
    /// @}

    // Flags 2
    /// Access to CODEC_FLAG2_* flags
    /// @{
    void setFlags2(int flags) noexcept;
    void addFlags2(int flags) noexcept;
    void clearFlags2(int flags) noexcept;
    int flags2() noexcept;
    bool isFlags2(int flags) noexcept;
    /// @}


protected:

    bool isValidForEncode(Direction direction, AVMediaType type) const noexcept;

    bool checkCodec(const Codec& codec, Direction direction, AVMediaType type, OptionalErrorCode ec);

    void open(const Codec &codec, AVDictionary **options, OptionalErrorCode ec);

    std::error_code checkDecodeEncodePreconditions() const noexcept;

public:

    template<typename T, typename Fn>
#if USE_CONCEPTS
        requires detail::valid_frame_type<T> &&
                 detail::invocable_r<dec_enc_handler_result_t, Fn, T>
#endif
    std::error_code decodeCommon(const class Packet &pkt, Fn frame_handler) noexcept
    {
        auto ec = checkDecodeEncodePreconditions();
        if (ec)
            return ec;

        auto ret = avcodec_send_packet(m_raw, pkt.raw());
        if (ret) {
            return {ret, ffmpeg_category()};
        }

        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            return make_error_code(Errors::CantAllocateFrame);
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(m_raw, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return {};
            else if (ret < 0)
                return {ret, ffmpeg_category()};

            // Wrap frame
            T frm(frame);
            // now, it safe to dereference frame
            av_frame_unref(frame);

            // Dial with PTS/DTS in packet/stream timebase

            if (pkt.timeBase() != Rational())
                frm.setTimeBase(pkt.timeBase());
            else
                frm.setTimeBase(m_stream.timeBase());

            AVFrame *frm_raw = frm.raw();

            if (frm_raw->pts == av::NoPts)
                frm_raw->pts = av::frame::get_best_effort_timestamp(frm_raw);

#if LIBAVCODEC_VERSION_MAJOR < 57
            if (frame->pts == av::NoPts)
                frame->pts = frame->pkt_pts;
#endif

            if (frm_raw->pts == av::NoPts)
                frm_raw->pts = frm_raw->pkt_dts;

            // Convert to decoder/frame time base. Seems not nessesary.
            frm.setTimeBase(timeBase());

            if (pkt)
                frm.setStreamIndex(pkt.streamIndex());
            else
                frm.setStreamIndex(m_stream.index());

            frm.setComplete(true);

            if constexpr (std::is_same_v<T, VideoFrame>) {
                frm.setPictureType(AV_PICTURE_TYPE_I);
            }

            if constexpr (std::is_same_v<T, AudioSamples>) {
                // Fix channels layout, only for legacy Channel Layout, new API assumes both parameters more sync
#if !API_NEW_CHANNEL_LAYOUT
                if (frm.channelsCount() && !frm.channelsLayout())
                    av::frame::set_channel_layout(frm.raw(), av_get_default_channel_layout(frm.channelsCount()));
#endif
            }

            if (auto const sts = frame_handler(std::move(frm)); sts.has_value()) {
                if (*sts == false)
                    break;
            } else {
                return sts.error();
            }
        }

        return {};
    }



    template<typename T, typename Fn>
#if USE_CONCEPTS
        requires detail::valid_frame_type<T> &&
                 detail::invocable_r<dec_enc_handler_result_t, Fn, Packet>
#endif
    std::error_code encodeCommon(const T &frame, Fn packet_handler) noexcept
    {
        auto ec = checkDecodeEncodePreconditions();
        if (ec)
            return ec;

        // avcodec_send_packet() can accept null packet or packet with zero size for flushing
        // but avcodec_send_frame() allow only null frame for flushing
        auto ret = avcodec_send_frame(m_raw, frame.isValid() ? frame.raw() : nullptr);
        if (ret) {
            return {ret, ffmpeg_category()};
        }

        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            return make_error_code(Errors::CantAllocatePacket);
        }

        // read all the available output packets (in general there may be any number of them
        while (ret >= 0) {
            ret = avcodec_receive_packet(m_raw, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return {};
            else if (ret < 0) {
                return {ret, ffmpeg_category()};
            }

            // Wrap packet (reference will be incremented)
            std::error_code ec;
            Packet pkt{packet, ec};
            if (ec) {
                return ec;
            }

            // Now it safe to unref packet
            av_packet_unref(packet);

            if (frame && frame.timeBase() != Rational()) {
                pkt.setTimeBase(frame.timeBase());
                pkt.setStreamIndex(frame.streamIndex());
            } else if (m_stream.isValid()) {
#if USE_CODECPAR
                pkt.setTimeBase(av_stream_get_codec_timebase(m_stream.raw()));
#else
                FF_DISABLE_DEPRECATION_WARNINGS
                    if (m_stream.raw()->codec) {
                    pkt.setTimeBase(m_stream.raw()->codec->time_base);
                }
                FF_ENABLE_DEPRECATION_WARNINGS
#endif
                pkt.setStreamIndex(m_stream.index());
            } else if (timeBase() != Rational()) {
                pkt.setTimeBase(timeBase());
            }

            pkt.setComplete(true);

            if (auto const sts = packet_handler(std::move(pkt)); sts.has_value()) {
                if (*sts == false)
                    break;
            } else {
                return sts.error();
            }
        }

        return {};
    }

private:
    Stream m_stream;
};


/**
 * @brief The GenericCodecContext class to copy contexts from input streams to output one.
 *
 * We should omit strong direction checking in this case. Only when we cast it to the appropriate
 * encoding coder.
 *
 */
class GenericCodecContext : public CodecContext2
{
protected:
    using CodecContext2::codecType;

public:
    GenericCodecContext() = default;

    GenericCodecContext(Stream st);

    GenericCodecContext(GenericCodecContext&& other);

    GenericCodecContext& operator=(GenericCodecContext&& rhs);

    AVMediaType codecType() const noexcept;
};


template<typename Clazz, Direction _direction, AVMediaType _type>
class CodecContextBase : public CodecContext2
{
protected:
    Clazz& moveOperator(Clazz &&rhs)
    {
        if (this == &rhs)
            return static_cast<Clazz&>(*this);
        Clazz(std::forward<Clazz>(rhs)).swap(static_cast<Clazz&>(*this));
        return static_cast<Clazz&>(*this);
    }

    using CodecContext2::setCodec;

public:

    using CodecContext2::_log;

    CodecContextBase()
        : CodecContext2()
    {
    }

    // Stream decoding/encoding
    explicit CodecContextBase(const class Stream &st, const class Codec& codec = Codec())
        : CodecContext2(st, codec, _direction, _type)
    {
    }

    // Stream independ decoding/encoding
    explicit CodecContextBase(const Codec &codec)
        : CodecContext2(codec, _direction, _type)
    {
    }

    //
    // Disable copy/Activate move
    //
    CodecContextBase(CodecContextBase &&other)
        : CodecContextBase()
    {
        swap(other);
    }
    //


    void setCodec(const Codec &codec, OptionalErrorCode ec = throws())
    {
        setCodec(codec, false, _direction, _type, ec);
    }

    void setCodec(const Codec &codec, bool resetDefaults, OptionalErrorCode ec = throws())
    {
        setCodec(codec, resetDefaults, _direction, _type, ec);
    }

    AVMediaType codecType() const noexcept
    {
        return CodecContext2::codecType(_type);
    }
};


template<typename Clazz, Direction _direction>
class VideoCodecContext : public CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_VIDEO>
{
public:
    using Parent = CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_VIDEO>;
    using Parent::Parent;
    using Parent::isValid;
    using Parent::isOpened;

    int width() const
    {
        return RAW_GET2(isValid(), width, 0);
    }

    int height() const
    {
        return RAW_GET2(isValid(), height, 0);
    }

    int codedWidth() const
    {
        return RAW_GET2(isValid(), coded_width, 0);
    }

    int codedHeight() const
    {
        return RAW_GET2(isValid(), coded_height, 0);
    }

    PixelFormat pixelFormat() const
    {
        return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
    }

    int32_t globalQuality() const
    {
        return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
    }

    int32_t gopSize() const
    {
        return RAW_GET2(isValid(), gop_size, 0);
    }

    int bitRateTolerance() const
    {
        return RAW_GET2(isValid(), bit_rate_tolerance, 0);
    }

    int maxBFrames() const
    {
        return RAW_GET2(isValid(), max_b_frames, 0);
    }

    Rational sampleAspectRatio() const
    {
        return RAW_GET(sample_aspect_ratio, AVRational());
    }

    void setWidth(int w) // Note, it also sets coded_width
    {
        if (isValid() && !isOpened())
        {
            m_raw->width       = w;
            m_raw->coded_width = w;
        }
    }

    void setHeight(int h) // Note, it also sets coded_height
    {
        if (isValid() && !isOpened())
        {
            m_raw->height       = h;
            m_raw->coded_height = h;
        }
    }

    void setCodedWidth(int w)
    {
        RAW_SET2(isValid() && !isOpened(), coded_width, w);
    }

    void setCodedHeight(int h)
    {
        RAW_SET2(isValid() && !isOpened(), coded_height, h);
    }

    void setPixelFormat(PixelFormat pixelFormat)
    {
        RAW_SET2(isValid(), pix_fmt, pixelFormat);
    }

    void setGlobalQuality(int32_t quality)
    {
        if (quality < 0 || quality > FF_LAMBDA_MAX)
            quality = FF_LAMBDA_MAX;

        RAW_SET2(isValid(), global_quality, quality);
    }

    void setGopSize(int32_t size)
    {
        RAW_SET2(isValid(), gop_size, size);
    }

    void setBitRateTolerance(int bitRateTolerance)
    {
        RAW_SET2(isValid(), bit_rate_tolerance, bitRateTolerance);
    }

    void setMaxBFrames(int maxBFrames)
    {
        RAW_SET2(isValid(), max_b_frames, maxBFrames);
    }

    void setSampleAspectRatio(const Rational& sampleAspectRatio)
    {
        RAW_SET(sample_aspect_ratio, sampleAspectRatio);
    }

protected:
    using Parent::moveOperator;
    using Parent::m_raw;
};


class VideoDecoderContext : public VideoCodecContext<VideoDecoderContext, Direction::Decoding>
{
public:
    using Parent = VideoCodecContext<VideoDecoderContext, Direction::Decoding>;
    using Parent::Parent;

    VideoDecoderContext() = default;
    VideoDecoderContext(VideoDecoderContext&& other);

    VideoDecoderContext& operator=(VideoDecoderContext&& other);

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, VideoFrame>
#endif
    void decode(const Packet &packet, Fn on_frame, OptionalErrorCode ec = throws())
    {
        clear_if(ec);
        auto sts = decodeCommon<VideoFrame>(packet, std::move(on_frame));
        if (sts) {
            throws_if(ec, sts.value(), sts.category());
        }
    }

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, VideoFrame>
#endif
    void decodeFlush(Fn on_frame, OptionalErrorCode ec = throws())
    {
        decode(Packet(), std::move(on_frame), ec);
    }
};


class VideoEncoderContext : public VideoCodecContext<VideoEncoderContext, Direction::Encoding>
{
public:
    using Parent = VideoCodecContext<VideoEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    VideoEncoderContext() = default;
    VideoEncoderContext(VideoEncoderContext&& other);

    VideoEncoderContext& operator=(VideoEncoderContext&& other);

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, Packet>
#endif
    void encode(const VideoFrame &frame, Fn packet_handler, OptionalErrorCode ec = throws())
    {
        clear_if(ec);
        auto sts = encodeCommon<VideoFrame>(frame, std::move(packet_handler));
        if (sts) {
            throws_if(ec, sts.value(), sts.category());
        }
    }

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, Packet>
#endif
    void encodeFlush(Fn packet_handler, OptionalErrorCode ec = throws())
    {
        encode(VideoFrame(nullptr), std::move(packet_handler), ec);
    }
};


template<typename Clazz, Direction _direction>
class AudioCodecContext : public CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_AUDIO>
{
public:
    using Parent = CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_AUDIO>;
    using Parent::Parent;
    using Parent::isValid;
    using Parent::isOpened;
    using Parent::_log;

    int sampleRate() const noexcept
    {
        return RAW_GET2(isValid(), sample_rate, 0);
    }

    int channels() const noexcept
    {
        if (!isValid())
            return 0;
        return codec_context::audio::get_channels(m_raw);
    }

    SampleFormat sampleFormat() const noexcept
    {
        return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
    }

    uint64_t channelLayout() const noexcept
    {
        if (!isValid())
            return 0;
        return codec_context::audio::get_channel_layout_mask(m_raw);
    }

#if API_NEW_CHANNEL_LAYOUT
    ChannelLayoutView channelLayout2() const noexcept
    {
        if (!isValid())
            return ChannelLayoutView{};
        return ChannelLayoutView{m_raw->ch_layout};
    }
#endif

    void setSampleRate(int sampleRate) noexcept
    {
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

    void setChannels(int channels) noexcept
    {
        if (!isValid() || channels <= 0)
            return;
        codec_context::audio::set_channels(m_raw, channels);
    }

    void setSampleFormat(SampleFormat sampleFormat) noexcept
    {
        RAW_SET2(isValid(), sample_fmt, sampleFormat);
    }

    void setChannelLayout(uint64_t layout) noexcept
    {
        if (!isValid() || layout == 0)
            return;
        codec_context::audio::set_channel_layout_mask(m_raw, layout);
    }

#if API_NEW_CHANNEL_LAYOUT
    void setChannelLayout(ChannelLayout layout) noexcept
    {
        if (!isValid() || !layout.isValid())
            return;
        m_raw->ch_layout = *layout.raw();
        layout.release(); // is controlled by the CodecContext
    }
#endif

protected:
    using Parent::moveOperator;
    using Parent::m_raw;
};


class AudioDecoderContext : public AudioCodecContext<AudioDecoderContext, Direction::Decoding>
{
public:
    using Parent = AudioCodecContext<AudioDecoderContext, Direction::Decoding>;
    using Parent::Parent;

    AudioDecoderContext() = default;
    AudioDecoderContext(AudioDecoderContext&& other);

    AudioDecoderContext& operator=(AudioDecoderContext&& other);

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, AudioSamples>
#endif
    void decode(const Packet &packet, Fn frame_handler, OptionalErrorCode ec = throws())
    {
        clear_if(ec);
        auto sts = decodeCommon<AudioSamples>(packet, std::move(frame_handler));
        if (sts) {
            throws_if(ec, sts.value(), sts.category());
        }
    }

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, AudioSamples>
#endif
    void decodeFlush(Fn frame_handler, OptionalErrorCode ec = throws())
    {
        decode(Packet(), std::move(frame_handler), ec);
    }
};


class AudioEncoderContext : public AudioCodecContext<AudioEncoderContext, Direction::Encoding>
{
public:
    using Parent = AudioCodecContext<AudioEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    AudioEncoderContext() = default;
    AudioEncoderContext(AudioEncoderContext&& other);

    AudioEncoderContext& operator=(AudioEncoderContext&& other);

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, Packet>
#endif
    void encode(const AudioSamples &samples, Fn packet_handler, OptionalErrorCode ec = throws())
    {
        clear_if(ec);
        auto sts = encodeCommon<AudioSamples>(samples, std::move(packet_handler));
        if (sts) {
            throws_if(ec, sts.value(), sts.category());
        }
    }

    template<typename Fn>
#if USE_CONCEPTS
        requires detail::invocable_r<dec_enc_handler_result_t, Fn, Packet>
#endif
    void encodeFlush(Fn packet_handler, OptionalErrorCode ec = throws())
    {
        encode(AudioSamples(nullptr), std::move(packet_handler), ec);
    }

};


} // namespace av
