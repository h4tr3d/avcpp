#pragma once

#include "ffmpeg.h"
#include "stream.h"
#include "frame.h"
#include "codec.h"
#include "dictionary.h"
#include "avutils.h"
#include "averror.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "avlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>
}

#include "codeccontext_deprecated.h"

namespace av {

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
    CodecContext2(const Stream &st,
                     const Codec& codec,
                     Direction direction,
                     AVMediaType type);

    // Stream independ decoding/encoding
    CodecContext2(const Codec &codec, Direction direction, AVMediaType type);

    ~CodecContext2();

    void setCodec(const Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, OptionalErrorCode ec = throws());

    AVMediaType codecType(AVMediaType contextType) const noexcept;

public:

    using BaseWrapper::_log;

    //
    // Common
    //

    void open(OptionalErrorCode ec = throws());
    void open(const Codec &codec, OptionalErrorCode ec = throws());
    void open(Dictionary &options, OptionalErrorCode ec = throws());
    void open(Dictionary &&options, OptionalErrorCode ec = throws());
    void open(Dictionary &options, const Codec &codec, OptionalErrorCode ec = throws());
    void open(Dictionary &&options, const Codec &codec, OptionalErrorCode ec = throws());

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
    int frameNumber() const noexcept;

    // Note, set ref counted to enable for multithreaded processing
    bool isRefCountedFrames() const noexcept;
    void setRefCountedFrames(bool refcounted) const noexcept;

    int strict() const noexcept;
    void setStrict(int strict) noexcept;

    int32_t bitRate() const noexcept;
    std::pair<int, int> bitRateRange() const noexcept;
    void setBitRate(int32_t bitRate) noexcept;
    void setBitRateRange(const std::pair<int, int> &bitRateRange) noexcept;

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


    std::pair<ssize_t, const std::error_category*>
    decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished,
                 int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *)) noexcept;

    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*)) noexcept;

public:
    template<typename T>
    std::pair<ssize_t, const std::error_category*>
    decodeCommon(T &outFrame,
                 const Packet &inPacket,
                 size_t offset,
                 int &frameFinished,
                 int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *))
    {
        auto st = decodeCommon(outFrame.raw(), inPacket, offset, frameFinished, decodeProc);
        if (std::get<1>(st))
            return st;

        if (!frameFinished)
            return std::make_pair(0u, nullptr);

        // Dial with PTS/DTS in packet/stream timebase

        if (inPacket.timeBase() != Rational())
            outFrame.setTimeBase(inPacket.timeBase());
        else
            outFrame.setTimeBase(m_stream.timeBase());

        AVFrame *frame = outFrame.raw();

        if (frame->pts == AV_NOPTS_VALUE)
            frame->pts = av_frame_get_best_effort_timestamp(frame);

        // Or: AVCODEC < 57.24.0 if this macro will be removes in future
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
    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket,
                 const T &inFrame,
                 int &gotPacket,
                 int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *))
    {
        auto st = encodeCommon(outPacket, inFrame.raw(), gotPacket, encodeProc);
        if (std::get<1>(st))
            return st;
        if (!gotPacket)
            return std::make_pair(0u, nullptr);

        if (inFrame && inFrame.timeBase() != Rational()) {
            outPacket.setTimeBase(inFrame.timeBase());
            outPacket.setStreamIndex(inFrame.streamIndex());
        } else if (m_stream.isValid()) {
#if USE_CODECPAR
            outPacket.setTimeBase(av_stream_get_codec_timebase(m_stream.raw()));
#else
            FF_DISABLE_DEPRECATION_WARNINGS
            if (m_stream.raw()->codec) {
                outPacket.setTimeBase(m_stream.raw()->codec->time_base);
            }
            FF_ENABLE_DEPRECATION_WARNINGS
#endif
            outPacket.setStreamIndex(m_stream.index());
        }

        // Recalc PTS/DTS/Duration
        if (m_stream.isValid()) {
            outPacket.setTimeBase(m_stream.timeBase());
        }

        outPacket.setComplete(true);

        return st;
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

    GenericCodecContext(Stream st)
        : CodecContext2(st, Codec(), st.direction(), st.mediaType())
    {
    }

    GenericCodecContext(GenericCodecContext&& other)
        : GenericCodecContext()
    {
        swap(other);
    }

    GenericCodecContext& operator=(GenericCodecContext&& rhs)
    {
        if (this == &rhs)
            return *this;
        GenericCodecContext(std::move(rhs)).swap(*this);
        return *this;
    }

    AVMediaType codecType() const noexcept
    {
        return codecType(stream().mediaType());
    }
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
    explicit CodecContextBase(const Stream &st, const Codec& codec = Codec())
        : CodecContext2(st, codec, _direction, _type)
    {
    }

    // Stream independ decoding/encoding
    explicit CodecContextBase(const Codec &codec)
        : CodecContext2(codec, _direction, _type)
    {
        if (checkCodec(codec, throws()))
            m_raw = avcodec_alloc_context3(codec.raw());
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
        return codecType(_type);
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

    int32_t globalQuality()
    {
        return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
    }

    int32_t gopSize()
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

    void setWidth(int w) // Note, it also sets coded_width
    {
        if (isValid() & !isOpened())
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

    /**
     * @brief decodeVideo  - decode video packet
     *
     * @param packet   packet to decode
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @param autoAllocateFrame  it true - output will be allocated at the ffmpeg internal, otherwise
     *                           it will be allocated before decode proc call.
     * @return encoded video frame, if error: exception thrown or error code returns, in both cases
     *         output undefined.
     */
    VideoFrame decode(const Packet    &packet,
                      OptionalErrorCode ec = throws(),
                      bool             autoAllocateFrame = true);

    /**
     * @brief decodeVideo - decode video packet with additional parameters
     *
     * @param[in] packet         packet to decode
     * @param[in] offset         data offset in packet
     * @param[out] decodedBytes  amount of decoded bytes
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @param autoAllocateFrame  it true - output will be allocated at the ffmpeg internal, otherwise
     *                           it will be allocated before decode proc call.
     * @return encoded video frame, if error: exception thrown or error code returns, in both cases
     *         output undefined.
     */
    VideoFrame decode(const Packet &packet,
                      size_t offset,
                      size_t &decodedBytes,
                      OptionalErrorCode ec = throws(),
                      bool    autoAllocateFrame = true);


private:
    VideoFrame decodeVideo(OptionalErrorCode ec,
                           const Packet &packet,
                           size_t offset,
                           size_t *decodedBytes,
                           bool    autoAllocateFrame);

};


class VideoEncoderContext : public VideoCodecContext<VideoEncoderContext, Direction::Encoding>
{
public:
    using Parent = VideoCodecContext<VideoEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    VideoEncoderContext() = default;
    VideoEncoderContext(VideoEncoderContext&& other);

    VideoEncoderContext& operator=(VideoEncoderContext&& other);

    /**
     * @brief encodeVideo - Flush encoder
     *
     * Stop flushing when returns empty packets
     *
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return
     */
    Packet encode(OptionalErrorCode ec = throws());

    /**
     * @brief encodeVideo - encode video frame
     *
     * @note Some encoders need some amount of frames before beginning encoding, so it is normal,
     *       that for some amount of frames returns empty packets.
     *
     * @param inFrame  frame to encode
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return
     */
    Packet encode(const VideoFrame &inFrame, OptionalErrorCode ec = throws());

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

        if (m_raw->channels)
            return m_raw->channels;

        if (m_raw->channel_layout)
            return av_get_channel_layout_nb_channels(m_raw->channel_layout);

        return 0;
    }

    SampleFormat sampleFormat() const noexcept
    {
        return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
    }

    uint64_t channelLayout() const noexcept
    {
        if (!isValid())
            return 0;

        if (m_raw->channel_layout)
            return m_raw->channel_layout;

        if (m_raw->channels)
            return av_get_default_channel_layout(m_raw->channels);

        return 0;
    }

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
        m_raw->channels = channels;
        if (m_raw->channel_layout != 0 ||
            av_get_channel_layout_nb_channels(m_raw->channel_layout) != channels) {
            m_raw->channel_layout = av_get_default_channel_layout(channels);
        }
    }

    void setSampleFormat(SampleFormat sampleFormat) noexcept
    {
        RAW_SET2(isValid(), sample_fmt, sampleFormat);
    }

    void setChannelLayout(uint64_t layout) noexcept
    {
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

    AudioSamples decode(const Packet &inPacket, OptionalErrorCode ec = throws());
    AudioSamples decode(const Packet &inPacket, size_t offset, OptionalErrorCode ec = throws());

};


class AudioEncoderContext : public AudioCodecContext<AudioEncoderContext, Direction::Encoding>
{
public:
    using Parent = AudioCodecContext<AudioEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    AudioEncoderContext() = default;
    AudioEncoderContext(AudioEncoderContext&& other);

    AudioEncoderContext& operator=(AudioEncoderContext&& other);

    Packet encode(OptionalErrorCode ec = throws());
    Packet encode(const AudioSamples &inSamples, OptionalErrorCode ec = throws());

};


} // namespace av
