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

template<typename Clazz, Direction _direction, AVMediaType _type>
class CodecContext2 : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
protected:
    void swap(Clazz &other)
    {
        using std::swap;
        swap(m_stream, other.m_stream);
        swap(m_raw, other.m_raw);
    }

    Clazz& moveOperator(Clazz &&rhs)
    {
        if (this == &rhs)
            return static_cast<Clazz&>(*this);
        Clazz(std::forward<Clazz>(rhs)).swap(static_cast<Clazz&>(*this));
        return static_cast<Clazz&>(*this);
    }

public:

    using BaseWrapper = FFWrapperPtr<AVCodecContext>;
    using BaseWrapper::BaseWrapper;

    CodecContext2()
    {
        m_raw = avcodec_alloc_context3(nullptr);
    }

    // Stream decoding/encoding
    explicit CodecContext2(const Stream &st, const Codec& codec = Codec())
        : m_stream(st)
    {
        if (st.direction() != _direction)
            throw av::Exception(make_avcpp_error(Errors::CodecInvalidDirection));

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
    }

    // Stream independ decoding/encoding
    explicit CodecContext2(const Codec &codec)
    {
        if (checkCodec(codec, throws()))
            m_raw = avcodec_alloc_context3(codec.raw());
    }

    ~CodecContext2()
    {
        std::error_code ec;
        close(ec);
        if (m_stream.isNull())
            av_freep(&m_raw);
    }

    //
    // Disable copy/Activate move
    // TBD
    //
    CodecContext2(Clazz &&other)
        : CodecContext2()
    {
        swap(other);
    }

#if 0
    Clazz& operator=(Clazz &&rhs)
    {
        if (this == &rhs)
            return *this;

        Clazz(std::move(rhs)).swap(*this);
        return *this;
    }
#endif
    //

    // Common
    void setCodec(const Codec &codec, std::error_code &ec = throws())
    {
        setCodec(codec, false, ec);
    }

    void setCodec(const Codec &codec, bool resetDefaults, std::error_code &ec = throws())
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

        if (!checkCodec(codec, ec))
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
            m_raw->codec_type = _type;
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


    void open(std::error_code &ec = throws())
    {
        open(Codec(), ec);
    }

    void open(const Codec &codec, std::error_code &ec = throws())
    {
        open(codec, nullptr, ec);
    }

    void open(Dictionary &options, std::error_code &ec = throws())
    {
        open(options, Codec(), ec);
    }

    void open(Dictionary &&options, std::error_code &ec = throws())
    {
        open(std::move(options), Codec(), ec);
    }

    void open(Dictionary &options, const Codec &codec, std::error_code &ec = throws())
    {
        auto prt = options.release();
        open(codec, &prt, ec);
        options.assign(prt);
    }

    void open(Dictionary &&options, const Codec &codec, std::error_code &ec = throws())
    {
        open(options, codec, ec);
    }

    void close(std::error_code &ec = throws())
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

    bool isOpened() const noexcept
    {
        return m_raw ? avcodec_is_open(m_raw) : false;
    }

    bool isValid() const noexcept
    {
        // Check parent stream first
        return ((m_stream.isValid() || m_stream.isNull()) && m_raw && m_raw->codec);
    }

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     * @return true if context copied, false otherwise
     */
    /// @{
    void copyContextFrom(const Clazz &other, std::error_code &ec = throws())
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
    /// @}

    Rational timeBase() const noexcept
    {
        return RAW_GET2(isValid(), time_base, AVRational());
    }

    void setTimeBase(const Rational &value) noexcept
    {
        RAW_SET2(isValid() && !isOpened(), time_base, value.getValue());
    }

    const Stream& stream() const noexcept
    {
        return m_stream;
    }

    Codec codec() const noexcept
    {
        if (isValid())
            return Codec(m_raw->codec);
        else
            return Codec();
    }

    AVMediaType codecType() const noexcept
    {
        if (isValid())
        {
            if (m_raw->codec && (m_raw->codec_type != m_raw->codec->type || m_raw->codec_type != _type))
                fflog(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type and/or context type\n");

            return m_raw->codec_type;
        }
        return _type;
    }

    void setOption(const std::string &key, const std::string &val, std::error_code &ec = throws())
    {
        setOption(key, val, 0, ec);
    }

    void setOption(const std::string &key, const std::string &val, int flags, std::error_code &ec = throws())
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

    //bool isAudio() const;
    //bool isVideo() const;

    // Common
    int frameSize() const noexcept
    {
        return RAW_GET2(isValid(), frame_size, 0);
    }

    int frameNumber() const noexcept
    {
        return RAW_GET2(isValid(), frame_number, 0);
    }

    // Note, set ref counted to enable for multithreaded processing
    bool isRefCountedFrames() const noexcept
    {
        return RAW_GET2(isValid(), refcounted_frames, false);
    }

    void setRefCountedFrames(bool refcounted) const noexcept
    {
        RAW_SET2(isValid() && !isOpened(), refcounted_frames, refcounted);
    }

    int strict() const
    {
        return RAW_GET2(isValid(), strict_std_compliance, 0);
    }

    void setStrict(int strict)
    {
        if (strict < FF_COMPLIANCE_EXPERIMENTAL)
            strict = FF_COMPLIANCE_EXPERIMENTAL;
        else if (strict > FF_COMPLIANCE_VERY_STRICT)
            strict = FF_COMPLIANCE_VERY_STRICT;

        RAW_SET2(isValid(), strict_std_compliance, strict);
    }

    int32_t bitRate() const
    {
        return RAW_GET2(isValid(), bit_rate, int32_t(0));
    }

    std::pair<int, int> bitRateRange() const
    {
        if (isValid())
            return std::make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
        else
            return std::make_pair(0, 0);
    }

    void setBitRate(int32_t bitRate)
    {
        RAW_SET2(isValid(), bit_rate, bitRate);
    }

    void setBitRateRange(const std::pair<int, int> &bitRateRange)
    {
        if (isValid())
        {
            m_raw->rc_min_rate = std::get<0>(bitRateRange);
            m_raw->rc_max_rate = std::get<1>(bitRateRange);
        }
    }

    // Flags
    /// Access to CODEC_FLAG_* flags
    /// @{
    void setFlags(int flags)
    {
        RAW_SET2(isValid(), flags, flags);
    }

    void addFlags(int flags)
    {
        if (isValid())
            m_raw->flags |= flags;
    }

    void clearFlags(int flags)
    {
        if (isValid())
            m_raw->flags &= ~flags;
    }

    int flags()
    {
        return RAW_GET2(isValid(), flags, 0);
    }

    bool isFlags(int flags)
    {
        if (isValid())
            return (m_raw->flags & flags);
        return false;
    }

    /// @}

    // Flags 2
    /// Access to CODEC_FLAG2_* flags
    /// @{
    void setFlags2(int flags)
    {
        RAW_SET2(isValid(), flags2, flags);
    }

    void addFlags2(int flags)
    {
        if (isValid())
            m_raw->flags2 |= flags;
    }

    void clearFlags2(int flags)
    {
        if (isValid())
            m_raw->flags2 &= ~flags;
    }

    int flags2()
    {
        return RAW_GET2(isValid(), flags2, 0);
    }

    bool isFlags2(int flags)
    {
        if (isValid())
            return (m_raw->flags2 & flags);
        return false;
    }
    /// @}

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

    bool isValidForEncode() const noexcept
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

        if (_direction == Direction::Decoding)
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

protected:
    bool checkCodec(const Codec& codec, std::error_code& ec)
    {
        if (_direction == Direction::Encoding && !codec.canEncode())
        {
            fflog(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
            throws_if(ec, Errors::CodecInvalidDirection);
            return false;
        }

        if (_direction == Direction::Decoding && !codec.canDecode())
        {
            fflog(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
            throws_if(ec, Errors::CodecInvalidDirection);
            return false;
        }

        if (_type != codec.type())
        {
            fflog(AV_LOG_ERROR, "Media type mismatch\n");
            throws_if(ec, Errors::CodecInvalidMediaType);
            return false;
        }

        return true;
    }

    void open(const Codec &codec, AVDictionary **options, std::error_code &ec)
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

    std::pair<ssize_t, const std::error_category*>
    make_error_pair(Errors errc)
    {
        return std::make_pair(static_cast<ssize_t>(errc), &avcpp_category());
    }

    std::pair<ssize_t, const std::error_category*>
    make_error_pair(ssize_t status)
    {
        if (status < 0)
            return std::make_pair(status, &ffmpeg_category());
        return std::make_pair(status, nullptr);
    }

    std::pair<ssize_t, const std::error_category*>
    decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished,
                 int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *))
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

    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*))
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
        return make_error_pair(stat);
    }

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

        //clog << "-------- pts: " << outPacket.raw()->pts << ", dts: " << outPacket.raw()->dts << '\n';

        // Recalc PTS/DTS/Duration
        if (m_stream.isValid()) {
            outPacket.setTimeBase(m_stream.timeBase());
        }

        outPacket.setComplete(true);

        return st;
    }

protected:
    Stream          m_stream;
};


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

//    template<typename T>
//    using Parent::template decodeCommonImpl<T>;

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
