#ifndef AV_CODECCONTEXT_H
#define AV_CODECCONTEXT_H

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
}

namespace av {

attribute_deprecated2("Use family of the VideoEncoderContext/VideoDecoderContext/AudioEncoderContext/AudioDecoderContext classes")
class CodecContext : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
private:
    void swap(CodecContext &other);

public:
    CodecContext();
    // Stream decoding/encoding
    explicit CodecContext(const Stream &st, const Codec& codec = Codec());
    // Stream independ decoding/encoding
    explicit CodecContext(const Codec &codec);
    ~CodecContext();

    // Disable copy/Activate move
    CodecContext(CodecContext &&other);
    CodecContext& operator=(CodecContext &&rhs);

    // Common
    void setCodec(const Codec &codec, std::error_code &ec = throws());
    void setCodec(const Codec &codec, bool resetDefaults, std::error_code &ec = throws());

    void open(std::error_code &ec = throws());
    void open(const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &options, std::error_code &ec = throws());
    void open(Dictionary &&options, std::error_code &ec = throws());
    void open(Dictionary &options, const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &&options, const Codec &codec, std::error_code &ec = throws());

    void close(std::error_code &ec = throws());

    bool isOpened() const noexcept;
    bool isValid() const noexcept;

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     * @return true if context copied, false otherwise
     */
    /// @{
    void copyContextFrom(const CodecContext &other, std::error_code &ec = throws());

    /// @}

    Rational timeBase() const;
    void     setTimeBase(const Rational &value);

    const Stream& stream() const;

    Codec       codec() const;
    AVMediaType codecType() const;

    void setOption(const std::string &key, const std::string &val, std::error_code &ec = throws());
    void setOption(const std::string &key, const std::string &val, int flags, std::error_code &ec = throws());

    bool isAudio() const;
    bool isVideo() const;

    // Common
    int frameSize() const;
    int frameNumber() const;

    // Note, set ref counted to enable for multithreaded processing
    bool isRefCountedFrames() const noexcept;
    void setRefCountedFrames(bool refcounted) const noexcept;

    // Video
    int                 width() const;
    int                 height() const;
    int                 codedWidth() const;
    int                 codedHeight() const;
    PixelFormat         pixelFormat() const;
    int32_t             bitRate() const;
    std::pair<int, int> bitRateRange() const;
    int32_t             globalQuality();
    int32_t             gopSize();
    int                 bitRateTolerance() const;
    int                 strict() const;
    int                 maxBFrames() const;

    void setWidth(int w); // Note, it also sets coded_width
    void setHeight(int h); // Note, it also sets coded_height
    void setCodedWidth(int w);
    void setCodedHeight(int h);
    void setPixelFormat(PixelFormat pixelFormat);
    void setBitRate(int32_t bitRate);
    void setBitRateRange(const std::pair<int, int> &bitRateRange);
    void setGlobalQuality(int32_t quality);
    void setGopSize(int32_t size);
    void setBitRateTolerance(int bitRateTolerance);
    void setStrict(int strict);
    void setMaxBFrames(int maxBFrames);

    // Audio
    int            sampleRate() const;
    int            channels() const;
    SampleFormat   sampleFormat() const;
    uint64_t       channelLayout() const;

    void        setSampleRate(int sampleRate);
    void        setChannels(int channels);
    void        setSampleFormat(SampleFormat sampleFormat);
    void        setChannelLayout(uint64_t layout);

    // Flags
    /// Access to CODEC_FLAG_* flags
    /// @{
    void        setFlags(int flags);
    void        addFlags(int flags);
    void        clearFlags(int flags);
    int         flags();
    bool        isFlags(int flags);
    /// @}

    // Flags 2
    /// Access to CODEC_FLAG2_* flags
    /// @{
    void        setFlags2(int flags2);
    void        addFlags2(int flags2);
    void        clearFlags2(int flags2);
    int         flags2();
    bool        isFlags2(int flags2);
    /// @}

    //
    // Video
    //

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
    VideoFrame decodeVideo(const Packet    &packet,
                            std::error_code &ec = throws(),
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
    VideoFrame decodeVideo(const Packet &packet,
                            size_t offset,
                            size_t &decodedBytes,
                            std::error_code &ec = throws(),
                            bool    autoAllocateFrame = true);

    /**
     * @brief encodeVideo - Flush encoder
     *
     * Stop flushing when returns empty packets
     *
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return
     */
    Packet encodeVideo(std::error_code &ec = throws());

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
    Packet encodeVideo(const VideoFrame &inFrame, std::error_code &ec = throws());

    //
    // Audio
    //
    AudioSamples decodeAudio(const Packet &inPacket, std::error_code &ec = throws());
    AudioSamples decodeAudio(const Packet &inPacket, size_t offset, std::error_code &ec = throws());

    Packet encodeAudio(std::error_code &ec = throws());
    Packet encodeAudio(const AudioSamples &inSamples, std::error_code &ec = throws());

    bool    isValidForEncode();

private:
    void open(const Codec &codec, AVDictionary **options, std::error_code &ec);

    VideoFrame decodeVideo(std::error_code &ec,
                            const Packet &packet,
                            size_t offset,
                            size_t *decodedBytes,
                            bool    autoAllocateFrame);

    std::pair<ssize_t, const std::error_category*>
    decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished,
                 int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *));

    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*));

    template<typename T>
    std::pair<ssize_t, const std::error_category*>
    decodeCommon(T &outFrame,
                 const Packet &inPacket,
                 size_t offset,
                 int &frameFinished,
                 int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *));

    template<typename T>
    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket,
                 const T &inFrame,
                 int &gotPacket,
                 int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *));

private:
    Direction       m_direction = Direction::Invalid;
    Stream          m_stream;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CodecContextBase : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
protected:
    void swap(CodecContextBase &other);

    //
    // No directly created
    //

    using BaseWrapper = FFWrapperPtr<AVCodecContext>;
    using BaseWrapper::BaseWrapper;

    CodecContextBase();

    // Stream decoding/encoding
    CodecContextBase(const Stream &st,
                     const Codec& codec,
                     Direction direction,
                     AVMediaType type);

    // Stream independ decoding/encoding
    CodecContextBase(const Codec &codec, Direction direction, AVMediaType type);

    ~CodecContextBase();

    void setCodec(const Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, std::error_code &ec = throws());

    AVMediaType codecType(AVMediaType contextType) const noexcept;

public:

    //
    // Common
    //

    void open(std::error_code &ec = throws());
    void open(const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &options, std::error_code &ec = throws());
    void open(Dictionary &&options, std::error_code &ec = throws());
    void open(Dictionary &options, const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &&options, const Codec &codec, std::error_code &ec = throws());

    void close(std::error_code &ec = throws());

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
    void copyContextFrom(const CodecContextBase &other, std::error_code &ec = throws());
    /// @}

    Rational timeBase() const noexcept;
    void setTimeBase(const Rational &value) noexcept;

    const Stream& stream() const noexcept;
    Codec codec() const noexcept;

    void setOption(const std::string &key, const std::string &val, std::error_code &ec = throws());
    void setOption(const std::string &key, const std::string &val, int flags, std::error_code &ec = throws());

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

    bool checkCodec(const Codec& codec, Direction direction, AVMediaType type, std::error_code& ec);

    void open(const Codec &codec, AVDictionary **options, std::error_code &ec);


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
            if (m_stream.raw()->codec) {
                outPacket.setTimeBase(m_stream.raw()->codec->time_base);
            }
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


template<typename Clazz, Direction _direction, AVMediaType _type>
class CodecContext2 : public CodecContextBase
{
protected:
    Clazz& moveOperator(Clazz &&rhs)
    {
        if (this == &rhs)
            return static_cast<Clazz&>(*this);
        Clazz(std::forward<Clazz>(rhs)).swap(static_cast<Clazz&>(*this));
        return static_cast<Clazz&>(*this);
    }

public:

    CodecContext2()
        : CodecContextBase()
    {
    }

    // Stream decoding/encoding
    explicit CodecContext2(const Stream &st, const Codec& codec = Codec())
        : CodecContextBase(st, codec, _direction, _type)
    {
    }

    // Stream independ decoding/encoding
    explicit CodecContext2(const Codec &codec)
        : CodecContextBase(codec, _direction, _type)
    {
        if (checkCodec(codec, throws()))
            m_raw = avcodec_alloc_context3(codec.raw());
    }

    //
    // Disable copy/Activate move
    //
    CodecContext2(Clazz &&other)
        : CodecContext2()
    {
        swap(other);
    }
    //


    void setCodec(const Codec &codec, std::error_code &ec = throws())
    {
        setCodec(codec, false, _direction, _type, ec);
    }

    void setCodec(const Codec &codec, bool resetDefaults, std::error_code &ec = throws())
    {
        setCodec(codec, resetDefaults, _direction, _type, ec);
    }

    AVMediaType codecType() const noexcept
    {
        return codecType(_type);
    }
};

#if 0
template<typename Clazz, Direction _direction, AVMediaType _type>
class CodecContext2 : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
    // Audio
#if 0
    int            sampleRate() const;
    int            channels() const;
    SampleFormat   sampleFormat() const;
    uint64_t       channelLayout() const;

    void        setSampleRate(int sampleRate);
    void        setChannels(int channels);
    void        setSampleFormat(SampleFormat sampleFormat);
    void        setChannelLayout(uint64_t layout);
#endif

    //
    // Audio
    //
    AudioSamples decodeAudio(const Packet &inPacket, std::error_code &ec = throws());
    AudioSamples decodeAudio(const Packet &inPacket, size_t offset, std::error_code &ec = throws());

    Packet encodeAudio(std::error_code &ec = throws());
    Packet encodeAudio(const AudioSamples &inSamples, std::error_code &ec = throws());
};
#endif


template<typename Clazz, Direction _direction>
class VideoCodecContext : public CodecContext2<Clazz, _direction, AVMEDIA_TYPE_VIDEO>
{
public:
    using Parent = CodecContext2<Clazz, _direction, AVMEDIA_TYPE_VIDEO>;
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
                      std::error_code &ec = throws(),
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
                      std::error_code &ec = throws(),
                      bool    autoAllocateFrame = true);


private:
    VideoFrame decodeVideo(std::error_code &ec,
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
    Packet encode(std::error_code &ec = throws());

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
    Packet encode(const VideoFrame &inFrame, std::error_code &ec = throws());

};


} // namespace av

#endif // AV_CODECCONTEXT_H
