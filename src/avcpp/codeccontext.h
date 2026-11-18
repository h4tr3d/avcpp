#pragma once

/**
 * Codec contexts: opening, configuring, encoding and decoding.
 *
 * @file
 * @ingroup codec
 */

#include <cstring>

#include "avcompat.h"

#include "ffmpeg.h"
#include "stream.h"
#include "avutils.h"
#include "averror.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "avlog.h"
#include "frame.h"
#include "subtitle.h"
#include "codec.h"
#include "channellayout.h"
#include "hwcontext.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}

namespace av {

namespace codec_context::audio {
void set_channels(AVCodecContext *obj, int channels);
void set_channel_layout_mask(AVCodecContext *obj, uint64_t mask);
int get_channels(const AVCodecContext *obj);
uint64_t get_channel_layout_mask(const AVCodecContext *obj);
}

namespace codec_context::internal {
const int *get_supported_samplerates(const struct AVCodec *codec);
}

/**
 * @brief Base class for audio and video codec contexts.
 *
 * @par Send/receive contract
 * The send/receive API wraps the FFmpeg send/receive model:
 *
 * - send() submits one packet (decoding) or one frame (encoding) and returns
 *   false when the codec cannot accept more input until the output is drained.
 *   This EAGAIN condition is not an error.
 * - receive() returns one output object. An empty (invalid) result means that
 *   no output is available yet (EAGAIN) or that the stream has ended.
 * - After the last input, call flush() once and keep calling receive() until it
 *   returns an empty result. At that point isEof() becomes true.
 * - An empty result is never marked as complete. Callers must check the object
 *   with operator bool() or isNull()/isValid().
 * - flushBuffers() resets the codec state and the EOF flag, for example after a
 *   seek back to a key frame.
 */
class CodecContext2 : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
protected:
    /**
     * Exchange the state of two contexts.
     * @param other
     */
    void swap(CodecContext2 &other);

    //
    // No directly created
    //

    /**
     * /// The reference-counting wrapper this context builds on.
     */
    using BaseWrapper = FFWrapperPtr<AVCodecContext>;
    using BaseWrapper::BaseWrapper;

    CodecContext2();

    // Stream decoding/encoding
#if AVCPP_HAS_AVFORMAT
    /**
     * Create a context attached to a stream.
     *
     * Used by the typed subclasses, which know the direction and media
     * type; not part of the public construction path.
     * @param st
     * @param codec
     * @param direction
     * @param type
     */
    CodecContext2(const class Stream &st,
                  const class Codec& codec,
                  Direction direction,
                  AVMediaType type);
#endif // if AVCPP_HAS_AVFORMAT

    // Stream independ decoding/encoding
    /**
     * Create a context for a codec without a stream.
     *
     * Used by the typed subclasses; not part of the public construction path.
     * @param codec
     * @param direction
     * @param type
     */
    CodecContext2(const class Codec &codec, Direction direction, AVMediaType type);

    ~CodecContext2();

    /**
     * Attach a codec and validate it against the direction and media type.
     *
     * The typed subclasses bind the direction and media type themselves.
     * @param codec
     * @param resetDefaults
     * @param direction
     * @param type
     * @param ec
     */
    void setCodec(const class Codec &codec, bool resetDefaults, Direction direction, AVMediaType type, OptionalErrorCode ec = throws());

    /**
     * Map a context media type to the effective media type.
     *
     * Codec contexts can be created for a media type that the codec
     * overrides; this resolves the final value.
     * @param contextType
     * @return the effective media type
     */
    AVMediaType codecType(AVMediaType contextType) const noexcept;

public:

    using BaseWrapper::_log;

    //
    // Common
    //

    /**
     * Open the codec with the parameters set so far.
     *
     * The codec is either the one passed to the constructor or the one set with
     * setCodec(). Everything that has to be known at open time — size, sample
     * rate, time base, hardware device or frames context — must be set before
     * this call.
     *
     * @param ec holder for the error code; `throws()` raises av::Exception
     */
    void open(OptionalErrorCode ec = throws());
    /**
     * Open the codec, choosing it explicitly.
     *
     * @param codec the codec to open; it must match the direction and media
     *              type of the context
     * @param ec    holder for the error code
     */
    void open(const Codec &codec, OptionalErrorCode ec = throws());
    /**
     * Open the codec with options.
     *
     * @param options codec options, for example {"threads", "4"}; the
     *                dictionary is not consumed and stays usable
     * @param ec      holder for the error code
     */
    void open(class Dictionary &options, OptionalErrorCode ec = throws());
    /**
     * Open the codec with options, taking ownership of the dictionary.
     *
     * @param options codec options, moved into the call
     * @param ec      holder for the error code
     */
    void open(class Dictionary &&options, OptionalErrorCode ec = throws());
    /**
     * Open an explicitly chosen codec with options.
     *
     * @param options codec options; not consumed
     * @param codec   the codec to open
     * @param ec      holder for the error code
     */
    void open(class Dictionary &options, const Codec &codec, OptionalErrorCode ec = throws());
    /**
     * Open an explicitly chosen codec with options, moving the dictionary.
     *
     * @param options codec options, moved into the call
     * @param codec   the codec to open
     * @param ec      holder for the error code
     */
    void open(class Dictionary &&options, const Codec &codec, OptionalErrorCode ec = throws());

    /**
     * Close the codec context.
     *
     * @deprecated Since FFmpeg 4.0 the recommendation is to destroy and recreate the context
     *             instead of closing it: close() does not free the context and leaves it in a
     *             half-initialized state.
     *
     * @param ec  holder for the error code; when it is not `throws()` an error is reported
     *            through it instead of an exception
     */
    [[deprecated("Start from FFmpeg 4.0 it is recommended to destroy and recreate codec context insted of close")]]
    void close(OptionalErrorCode ec = throws());

    /**
     * Whether the codec has been opened.
     *
     * @return `true` after a successful open() and before the context dies
     */
    bool isOpened() const noexcept;
    /**
     * Whether the context owns an FFmpeg codec context.
     *
     * @return `true` when the underlying AVCodecContext exists
     */
    bool isValid() const noexcept;

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     * @param ec     holder for the error code
     */
    /// @{
    void copyContextFrom(const CodecContext2 &other, OptionalErrorCode ec = throws());
    /// @}

    /**
     * Time base of the codec.
     *
     * @return timestamps in this context are expressed in these units
     */
    Rational timeBase() const noexcept;
    /**
     * Set the time base.
     *
     * @param value timestamp units per second; must be set before open()
     */
    void setTimeBase(const Rational &value) noexcept;

    /**
     * @brief Frame rate advertised by the codec.
     *
     * Hardware encoders (for example h264_qsv) read this value and need it to
     * be set before open().
     * @return the frame rate, or an empty value when it is not set
     */
    Rational frameRate() const noexcept;
    /**
     * Set the frame rate.
     *
     * @param value frames per second; some encoders (QSV, for one) require it
     *              before open(), others use it as a hint
     */
    void setFrameRate(const Rational &value) noexcept;

#if AVCPP_NEW_CODEC_API
    /**
     * @brief Returns true if the codec has been flushed and all output is drained.
     *
     * An empty result of receive() with isEof() == false means that more input is
     * required (EAGAIN), for example during the initial delay of a reordering
     * decoder.
     * @return `true` when the codec is flushed and fully drained
     */
    bool isEof() const noexcept { return m_eof; }

    /**
     * @brief Resets the codec internal state and the end-of-stream flag.
     *
     * Wraps avcodec_flush_buffers(). Use it to reuse the context after EOF or to
     * seek back to a key frame.
     */
    void flushBuffers() noexcept
    {
        if (m_raw)
            avcodec_flush_buffers(m_raw);
        m_eof = false;
    }
#endif

#if AVCPP_HAS_AVFORMAT
    /**
     * The stream this context is attached to.
     *
     * @return the stream passed to the constructor, or an empty stream when the
     *         context was created from a codec alone
     */
    const Stream& stream() const noexcept;
#endif // if AVCPP_HAS_AVFORMAT
    /**
     * The codec in use.
     *
     * @return the codec descriptor, or an invalid one when no codec is set
     */
    Codec codec() const noexcept;

    /**
     * Set a private codec option.
     *
     * @param key option name, as accepted by `ffmpeg -h`
     * @param val option value
     * @param ec  holder for the error code; an unknown key reports
     *            Errors::InvalidArgument
     */
    void setOption(const std::string &key, const std::string &val, OptionalErrorCode ec = throws());
    /**
     * Set a private codec option with AVDictionary flags.
     *
     * @param key   option name
     * @param val   option value
     * @param flags combination of `AV_DICT_*` flags, for example
     *              `AV_DICT_DONT_OVERWRITE`
     * @param ec    holder for the error code
     */
    void setOption(const std::string &key, const std::string &val, int flags, OptionalErrorCode ec = throws());

    /**
     * Number of samples per frame the codec works with.
     *
     * @return the frame size in samples; zero for codecs that do not use it
     */
    int frameSize() const noexcept;
    /**
     * Counter of frames passed through the context.
     *
     * @return the frame number reported by FFmpeg, or zero when unavailable
     */
    int64_t frameNumber() const noexcept;

    // Note, set ref counted to enable for multithreaded processing
    /**
     * Whether the codec hands out reference-counted frames.
     *
     * @return `true` when frames can outlive the next receive() call
     */
    bool isRefCountedFrames() const noexcept;
    /**
     * Ask the codec for reference-counted frames.
     *
     * @param refcounted `true` makes frame copies cheap and keeps a frame valid
     *                   after the next decode call, which is required for
     *                   multithreaded processing
     */
    void setRefCountedFrames(bool refcounted) const noexcept;

    /**
     * Current strictness level of the codec.
     *
     * @return the `FF_COMPLIANCE_*` value in use
     */
    int strict() const noexcept;
    /**
     * Set the strictness level.
     *
     * @param strict an `FF_COMPLIANCE_*` value; higher levels make the codec
     *               reject streams that are not fully standard compliant
     */
    void setStrict(int strict) noexcept;

    /**
     * Target bit rate.
     *
     * @return bits per second
     */
    int64_t bitRate() const noexcept;
    /**
     * Allowed bit rate range.
     *
     * @return minimum and maximum bit rate in bits per second
     */
    std::pair<int64_t, int64_t> bitRateRange() const noexcept;
    /**
     * Set the target bit rate.
     *
     * @param bitRate bits per second; encoders clamp it to the range they
     *                support
     */
    void setBitRate(int64_t bitRate) noexcept;
    /**
     * Set the allowed bit rate range.
     *
     * @param bitRateRange minimum and maximum in bits per second
     */
    void setBitRateRange(const std::pair<int64_t, int64_t> &bitRateRange) noexcept;

    // Flags
    /// Access to CODEC_FLAG_* flags
    /// @{
    /**
     * Replace the `AV_CODEC_FLAG_*` set.
     *
     * @param flags the new flag mask
     */
    void setFlags(int flags) noexcept;
    /**
     * Add `AV_CODEC_FLAG_*` flags.
     *
     * @param flags flags to set
     */
    void addFlags(int flags) noexcept;
    /**
     * Remove `AV_CODEC_FLAG_*` flags.
     *
     * @param flags flags to clear
     */
    void clearFlags(int flags) noexcept;
    /**
     * Current `AV_CODEC_FLAG_*` mask.
     *
     * @return the flags in use
     */
    int flags() noexcept;
    /**
     * Whether all given flags are set.
     *
     * @param flags flags to test
     *
     * @return `true` when every flag in the mask is present
     */
    bool isFlags(int flags) noexcept;
    /// @}

    // Flags 2
    /// Access to CODEC_FLAG2_* flags
    /// @{
    /**
     * Replace the `AV_CODEC_FLAG2_*` set.
     *
     * @param flags the new flag mask
     */
    void setFlags2(int flags) noexcept;
    /**
     * Add `AV_CODEC_FLAG2_*` flags.
     *
     * @param flags flags to set
     */
    void addFlags2(int flags) noexcept;
    /**
     * Remove `AV_CODEC_FLAG2_*` flags.
     *
     * @param flags flags to clear
     */
    void clearFlags2(int flags) noexcept;
    /**
     * Current `AV_CODEC_FLAG2_*` mask.
     *
     * @return the flags in use
     */
    int flags2() noexcept;
    /**
     * Whether all given flags are set.
     *
     * @param flags flags to test
     *
     * @return `true` when every flag in the mask is present
     */
    bool isFlags2(int flags) noexcept;
    /// @}


protected:

    /**
     * Whether the configured codec can encode the given media type.
     *
     * Validation helper used by setCodec() and open().
     * @param direction
     * @param type
     * @return `true` when the codec supports this combination
     */
    bool isValidForEncode(Direction direction, AVMediaType type) const noexcept;

    /**
     * Validate a codec against a direction and media type, reporting the reason.
     *
     * @param codec
     * @param direction
     * @param type
     * @param ec
     * @return `true` when the codec is usable
     */
    bool checkCodec(const Codec& codec, Direction direction, AVMediaType type, OptionalErrorCode ec);

    /**
     * Open the codec with raw FFmpeg options.
     *
     * The public open() overloads convert a Dictionary and call this one.
     * @param codec
     * @param options
     * @param ec
     */
    void open(const Codec &codec, AVDictionary **options, OptionalErrorCode ec);

    // Hardware codec state. The device and frames contexts are reference
    // counted, copying the wrappers is cheap.
    /**
     * Device attached by setHwDeviceContext() or configureHwEncoding().
     */
    hw::DeviceContext m_hwDevice;
    /**
     * Pool attached by setHwFramesContext() or configureHwEncoding().
     */
    hw::FramesContext m_hwFrames;
    /**
     * Whether decoded hardware frames are copied to system memory automatically.
     */
    bool              m_hwTransferToSystem = false;
    /**
     * Whether software frames are uploaded into the pool before encoding.
     */
    bool              m_hwAutoUpload       = true;
    /**
     * How the encoder receives hardware frames.
     */
    hw::EncodingMode  m_hwEncodingMode     = hw::EncodingMode::Auto;

    std::pair<int, const std::error_category*>
    /**
     * Decode one packet into an FFmpeg frame.
     *
     * The typed contexts wrap this and convert the result.
     * @param outFrame
     * @param inPacket
     * @param offset
     * @param frameFinished
     * @param decodeProc
     * @return the FFmpeg error code and the category it belongs to
     */
    decodeCommon(AVFrame *outFrame, const class Packet &inPacket, size_t offset, int &frameFinished,
                 int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *)) noexcept;

    std::pair<int, const std::error_category*>
    /**
     * Encode one typed frame into a packet.
     *
     * Shared implementation of the public encode() overloads.
     * @param outPacket
     * @param inFrame
     * @param gotPacket
     * @param encodeProc
     * @return the FFmpeg error code and the category it belongs to
     */
    encodeCommon(class Packet &outPacket, const AVFrame *inFrame, int &gotPacket,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*)) noexcept;

#if AVCPP_NEW_CODEC_API
    // Decode
    /**
     * Submit a packet to a decoder.
     *
     * Shared implementation of send() for decoding contexts.
     * @param pkt
     * @return an FFmpeg error code, empty on success
     */
    std::error_code sendCommon(const class Packet& pkt);
    /**
     * Read one frame from a decode context.
     *
     * Shared implementation of receive() for video decoders.
     * @param frame
     * @return an FFmpeg error code, empty on success
     */
    std::error_code receiveCommon(AVFrame *frame);
    template<typename T>
    /**
     * Read one typed output object from a decode context.
     *
     * Shared implementation of receive() for the typed decoders.
     * @param frame
     * @return an FFmpeg error code, empty on success
     */
    std::error_code receiveCommon(T &frame);

    // Encode
    /**
     * Submit a frame to an encoder.
     *
     * Shared implementation of send() for video encoders.
     * @param frame
     * @return an FFmpeg error code, empty on success
     */
    std::error_code sendCommon(const AVFrame* frame);
    /**
     * Read one packet from an encode context.
     *
     * Shared implementation of receive() for encoders.
     * @param pkt
     * @return an FFmpeg error code, empty on success
     */
    std::error_code receiveCommon(class Packet& pkt);
    template<typename T>
    /**
     * Submit a typed frame to an encoder.
     *
     * Shared implementation of send() for the typed encoders.
     * @param frame
     * @return an FFmpeg error code, empty on success
     */
    std::error_code sendCommon(const T& frame);
#endif

public:
    template<typename T>
    std::pair<int, const std::error_category*>
    /**
     * Decode one packet into a typed frame.
     *
     * Shared implementation of the public decode() overloads.
     * @param outFrame
     * @param inPacket
     * @param offset
     * @param frameFinished
     * @param decodeProc
     * @return the FFmpeg error code and the category it belongs to
     */
    decodeCommon(T &outFrame,
                 const class Packet &inPacket,
                 size_t offset,
                 int &frameFinished,
                 int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *));

    template<typename T>
    std::pair<int, const std::error_category*>
    /**
     * Encode one typed frame into a packet.
     *
     * Shared implementation of the public encode() overloads.
     * @param outPacket
     * @param inFrame
     * @param gotPacket
     * @param encodeProc
     * @return the FFmpeg error code and the category it belongs to
     */
    encodeCommon(class Packet &outPacket,
                 const T &inFrame,
                 int &gotPacket,
                 int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *));

#if AVCPP_NEW_CODEC_API
    // Decode
    /**
     * Submit a packet to a decoder.
     *
     * @param packet the compressed packet
     * @param ec     holder for the error code
     *
     * @return `false` when the decoder cannot accept more input until its
     *         output is drained; this EAGAIN condition is not an error
     */
    bool send(const Packet &packet, OptionalErrorCode ec = throws());

    /**
     * Read one decoded packet from an encoder.
     *
     * @param ec holder for the error code
     *
     * @return an empty packet when nothing is available yet (EAGAIN) or the
     *         stream has ended; use isEof() to tell the two apart
     */
    Packet receive(OptionalErrorCode ec = throws());
#endif

private:
#if AVCPP_HAS_AVFORMAT
    Stream m_stream;
#endif // if AVCPP_HAS_AVFORMAT
    Direction m_direction{Direction::Invalid};
#if AVCPP_NEW_CODEC_API
    bool m_eof{false};
#endif
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
    /**
     * Create an empty context; no codec and no stream.
     */
    GenericCodecContext() = default;

#if AVCPP_HAS_AVFORMAT
    /**
     * Attach the context to a stream without a direction.
     *
     * Used to copy parameters from an input stream to an output one during
     * remuxing, where neither encoding nor decoding happens.
     *
     * @param st the stream to copy parameters from
     */
    GenericCodecContext(Stream st);
#endif // if AVCPP_HAS_AVFORMAT

    /**
     * Move the context.
     *
     * @param other the context to move from; left empty
     */
    GenericCodecContext(GenericCodecContext&& other);

    /**
     * Move assignment.
     *
     * @param rhs the context to move from; left empty
     *
     * @return this context
     */
    GenericCodecContext& operator=(GenericCodecContext&& rhs);

    /**
     * The media type of the attached stream or codec.
     *
     * @return the media type, or `AVMEDIA_TYPE_UNKNOWN` when nothing is set
     */
    AVMediaType codecType() const noexcept;
};


template<typename Clazz, Direction _direction, AVMediaType _type>
/**
 * Base class that fixes the direction and media type of a codec context.
 *
 * It supplies the constructors, setCodec() and flush() with the direction and
 * media type already bound, so the concrete contexts are thin.
 */
class CodecContextBase : public CodecContext2
{
protected:
    /**
     * Move-assign the derived object and return it.
     *
     * Lets the concrete contexts share one move-assignment expression.
     * @param rhs
     * @return the assigned object
     */
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

    /**
     * Create an empty context of this direction and media type.
     */
    CodecContextBase()
        : CodecContext2()
    {
    }

    // Stream decoding/encoding
#if AVCPP_HAS_AVFORMAT
    /**
     * Create a context attached to a stream.
     *
     * @param st    the stream whose codec parameters are used
     * @param codec the codec to use; when empty, it is taken from the stream
     */
    explicit CodecContextBase(const class Stream &st, const class Codec& codec = Codec())
        : CodecContext2(st, codec, _direction, _type)
    {
    }
#endif // if AVCPP_HAS_AVFORMAT

    // Stream independ decoding/encoding
    /**
     * Create a context for a codec, without a stream.
     *
     * @param codec the codec to use
     */
    explicit CodecContextBase(const Codec &codec)
        : CodecContext2(codec, _direction, _type)
    {
    }

    //
    // Disable copy/Activate move
    //
    /**
     * Move the context.
     *
     * @param other the context to move from; left empty
     */
    CodecContextBase(CodecContextBase &&other)
        : CodecContextBase()
    {
        swap(other);
    }
    //


    /**
     * Choose the codec without resetting its defaults.
     *
     * @param codec the codec to use; must match the direction and media type
     * @param ec    holder for the error code
     */
    void setCodec(const Codec &codec, OptionalErrorCode ec = throws())
    {
        setCodec(codec, false, _direction, _type, ec);
    }

    /**
     * Choose the codec.
     *
     * @param codec         the codec to use
     * @param resetDefaults `true` resets the context to the codec defaults,
     *                      discarding settings made before this call
     * @param ec            holder for the error code
     */
    void setCodec(const Codec &codec, bool resetDefaults, OptionalErrorCode ec = throws())
    {
        setCodec(codec, resetDefaults, _direction, _type, ec);
    }

    /**
     * The media type of this context.
     *
     * @return `_type`, the media type the template was instantiated with
     */
    AVMediaType codecType() const noexcept
    {
        return CodecContext2::codecType(_type);
    }

#if AVCPP_NEW_CODEC_API

    /**
     * @brief Puts the codec into draining mode.
     *
     * After a successful flush() the caller must drain the output with repeated
     * receive() calls until an empty result is returned. When the draining is
     * complete, isEof() returns true.
     *
     * @return true if draining was initiated (or the codec is already drained),
     *         false if the output queue is full and must be drained first.
     * @param ec
     */
    bool flush(OptionalErrorCode ec = throws())
    {
        clear_if(ec);

        if constexpr (_type == AVMEDIA_TYPE_SUBTITLE) {
            throws_if(ec, Errors::CodecUnsupportedOperation);
            return false;
        }

        if (!isValid()) {
            throws_if(ec, make_error_code(Errors::CodecInvalid));
            return false;
        }

        if (!isOpened()) {
            throws_if(ec, make_error_code(Errors::CodecNotOpened));
            return false;
        }

        int ret = 0;
        if constexpr (_direction == Direction::Decoding) {
            ret = avcodec_send_packet(m_raw, nullptr);
        } else {
            ret = avcodec_send_frame(m_raw, nullptr);
        }

        if (ret == AVERROR(EAGAIN)) {
            // Output queue is full: drain it first, then call flush() again
            return false;
        }
        if (ret == AVERROR_EOF) {
            // Already drained, flush is idempotent
            return true;
        }
        if (ret) {
            throws_if(ec, ret, ffmpeg_category());
            return false;
        }
        return true;
    }

#endif
};


template<typename Clazz, Direction _direction>
/**
 * Video-specific parameters shared by video encoders and decoders.
 *
 * Width, height, pixel format, GOP size and bit rate are plain accessors over
 * AVCodecContext fields: they do no I/O, and they are ignored once the codec is
 * open where FFmpeg does not allow changes.
 */
class VideoCodecContext : public CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_VIDEO>
{
public:
    /// The base class this context derives from.
    using Parent = CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_VIDEO>;
    using Parent::Parent;
    using Parent::isValid;
    using Parent::isOpened;

    /**
     * Frame width stored in the context.
     *
     * @return the width in pixels, or zero when the context is not valid
     */
    int width() const
    {
        return RAW_GET2(isValid(), width, 0);
    }

    /**
     * Frame height stored in the context.
     *
     * @return the height in pixels, or zero when the context is not valid
     */
    int height() const
    {
        return RAW_GET2(isValid(), height, 0);
    }

    /**
     * Width as coded by the bitstream, before any cropping.
     *
     * @return the coded width in pixels, or zero when the context is not valid
     */
    int codedWidth() const
    {
        return RAW_GET2(isValid(), coded_width, 0);
    }

    /**
     * Height as coded by the bitstream, before any cropping.
     *
     * @return the coded height in pixels, or zero when the context is not valid
     */
    int codedHeight() const
    {
        return RAW_GET2(isValid(), coded_height, 0);
    }

    /**
     * Pixel format the codec works with.
     *
     * @return the format, or `AV_PIX_FMT_NONE` when the context is not valid
     */
    PixelFormat pixelFormat() const
    {
        return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
    }

    /**
     * Global quality value used by constant-quality encoders.
     *
     * @return the quality in the `FF_LAMBDA_*` scale
     */
    int32_t globalQuality() const
    {
        return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
    }

    /**
     * Distance between key frames.
     *
     * @return the GOP size in frames; zero means the codec default
     */
    int32_t gopSize() const
    {
        return RAW_GET2(isValid(), gop_size, 0);
    }

    /**
     * How far the encoder may deviate from the target bit rate.
     *
     * @return the tolerance in bits per second
     */
    int bitRateTolerance() const
    {
        return RAW_GET2(isValid(), bit_rate_tolerance, 0);
    }

    /**
     * Maximum number of B-frames the encoder may use.
     *
     * @return the limit; zero disables B-frames
     */
    int maxBFrames() const
    {
        return RAW_GET2(isValid(), max_b_frames, 0);
    }

    /**
     * Pixel aspect ratio of the frames.
     *
     * @return the ratio, or an empty value when it is not set
     */
    Rational sampleAspectRatio() const
    {
        return RAW_GET(sample_aspect_ratio, AVRational());
    }

    /**
     * Set the frame width.
     *
     * Also sets the coded width, which is what a decoder expects when the
     * stream has no cropping. Ignored after open().
     *
     * @param w width in pixels
     */
    void setWidth(int w) // Note, it also sets coded_width
    {
        if (isValid() && !isOpened())
        {
            m_raw->width       = w;
            m_raw->coded_width = w;
        }
    }

    /**
     * Set the frame height.
     *
     * Also sets the coded height. Ignored after open().
     *
     * @param h height in pixels
     */
    void setHeight(int h) // Note, it also sets coded_height
    {
        if (isValid() && !isOpened())
        {
            m_raw->height       = h;
            m_raw->coded_height = h;
        }
    }

    /**
     * Set the coded width independently of the display width.
     *
     * Ignored after open().
     *
     * @param w coded width in pixels
     */
    void setCodedWidth(int w)
    {
        RAW_SET2(isValid() && !isOpened(), coded_width, w);
    }

    /**
     * Set the coded height independently of the display height.
     *
     * Ignored after open().
     *
     * @param h coded height in pixels
     */
    void setCodedHeight(int h)
    {
        RAW_SET2(isValid() && !isOpened(), coded_height, h);
    }

    /**
     * Set the pixel format.
     *
     * @param pixelFormat the format the codec will be fed with, or asked to
     *                    produce
     */
    void setPixelFormat(PixelFormat pixelFormat)
    {
        RAW_SET2(isValid(), pix_fmt, pixelFormat);
    }

    /**
     * Set the global quality for constant-quality encoders.
     *
     * @param quality quality in the `FF_LAMBDA_*` scale; values outside the
     *                range are clamped to `FF_LAMBDA_MAX`
     */
    void setGlobalQuality(int32_t quality)
    {
        if (quality < 0 || quality > FF_LAMBDA_MAX)
            quality = FF_LAMBDA_MAX;

        RAW_SET2(isValid(), global_quality, quality);
    }

    /**
     * Set the distance between key frames.
     *
     * @param size GOP size in frames; zero leaves it to the codec
     */
    void setGopSize(int32_t size)
    {
        RAW_SET2(isValid(), gop_size, size);
    }

    /**
     * Set how far the encoder may deviate from the target bit rate.
     *
     * @param bitRateTolerance tolerance in bits per second
     */
    void setBitRateTolerance(int bitRateTolerance)
    {
        RAW_SET2(isValid(), bit_rate_tolerance, bitRateTolerance);
    }

    /**
     * Limit the number of B-frames.
     *
     * @param maxBFrames the limit; zero disables B-frames
     */
    void setMaxBFrames(int maxBFrames)
    {
        RAW_SET2(isValid(), max_b_frames, maxBFrames);
    }

    /**
     * Set the pixel aspect ratio.
     *
     * @param sampleAspectRatio width divided by height of a single pixel
     */
    void setSampleAspectRatio(const Rational& sampleAspectRatio)
    {
        RAW_SET(sample_aspect_ratio, sampleAspectRatio);
    }

protected:
    using Parent::moveOperator;
    using Parent::m_raw;
};


/**
 * Decodes video packets into frames.
 *
 * Uses the send/receive API; see the class comment of CodecContext2 for the
 * contract. The legacy decode() overloads remain available for software
 * decoders and are convenient for one-shot conversions.
 */
class VideoDecoderContext : public VideoCodecContext<VideoDecoderContext, Direction::Decoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = VideoCodecContext<VideoDecoderContext, Direction::Decoding>;
    using Parent::Parent;

    /**
     * Create an empty decoder; set a codec and open() it later.
     */
    VideoDecoderContext() = default;
    /**
     * Move the decoder.
     *
     * @param other the decoder to move from; left empty
     */
    VideoDecoderContext(VideoDecoderContext&& other);

    /**
     * Move assignment.
     *
     * @param other the decoder to move from; left empty
     *
     * @return this decoder
     */
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


#if AVCPP_NEW_CODEC_API
    /**
     * @brief Sends a packet to the decoder.
     *
     * @return false if the decoder cannot accept more input until receive()
     *         is called. This EAGAIN condition is not an error.
     * @param ec
     * @param packet
     */
    bool send(const Packet &packet, OptionalErrorCode ec = throws());

    /**
     * @brief Receives one decoded frame.
     *
     * Returns an invalid frame when no frame is available yet (EAGAIN) and
     * when the stream has ended. Use isEof() to distinguish the two cases.
     *
     * @note avcodec_receive_frame() always writes into the frame allocated by
     *       the decoder, so @p autoAllocateFrame only affects the wrapper
     *       object and is kept for compatibility.
     *
     * @param ec                holder for the error code
     * @param autoAllocateFrame kept for compatibility; see the note above
     *
     * @return the decoded frame, or an invalid frame when nothing is available
     *         yet or the stream has ended
     */
    VideoFrame receive(OptionalErrorCode ec = throws(), bool autoAllocateFrame = true);
#endif

    /**
     * @brief Configures a hardware device for decoding.
     *
     * Must be called after the codec is set and before open(). The decoder uses
     * the device to choose the output pixel format; there is no silent software
     * fallback. If the codec has no hardware configuration for the device type,
     * open() fails with Errors::HardwareConfigNotFound.
     *
     * The function installs an internal get_format() callback. It does not use
     * AVCodecContext::opaque.
     * @param device
     * @param ec
     */
    void setHwDeviceContext(const hw::DeviceContext& device,
                            OptionalErrorCode ec = throws());

    /**
     * @brief Removes the hardware device and restores the default get_format().
     */
    void clearHwDeviceContext() noexcept;

    /**
     * @brief Device context set with setHwDeviceContext().
     */
    /**
     * Device configured for this decoder.
     *
     * @return the device set with setHwDeviceContext(), or an invalid value
     *         when decoding on the CPU
     */
    hw::DeviceContext hwDeviceContext() const noexcept;

    /**
     * @brief Frames context of decoded hardware frames.
     *
     * Returns the frames context owned by the decoder, if any. It exists when
     * the codec needs one (avcpp derives it for codecs that advertise only
     * AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, for example QSV in FFmpeg 4.x)
     * or when FFmpeg creates it for a hardware accelerated decoder while
     * decoding the first packet.
     *
     * The method may return an invalid context before that point, and for
     * decoders that keep the context only inside the decoded frames.
     * VideoFrame::hwFramesContext() always returns the context of a produced
     * frame.
     *
     * @return the frames context owned by the decoder, or an invalid value when
     *         there is none yet
     */
    hw::FramesContext hwFramesContext() const noexcept;

    /**
     * @brief Transfer decoded hardware frames to system memory automatically.
     *
     * Disabled by default. When enabled, decode() and receive() return software
     * frames. Keep it disabled for zero-copy processing.
     * @param enabled
     */
    void setHwTransferToSystem(bool enabled) noexcept;
    /**
     * Whether decoded hardware frames are transferred to system memory.
     *
     * @return the value set by setHwTransferToSystem()
     */
    bool isHwTransferToSystem() const noexcept;

private:
    VideoFrame decodeVideo(OptionalErrorCode ec,
                           const Packet &packet,
                           size_t offset,
                           size_t *decodedBytes,
                           bool    autoAllocateFrame);

};


/**
 * Encodes video frames into packets.
 *
 * Frames must carry the encoder time base before they are passed to encode()
 * or send(); a mismatch produces packets with meaningless timestamps rather
 * than an error.
 */
class VideoEncoderContext : public VideoCodecContext<VideoEncoderContext, Direction::Encoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = VideoCodecContext<VideoEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    /**
     * Create an empty encoder; set a codec and open() it later.
     */
    VideoEncoderContext() = default;
    /**
     * Move the encoder.
     *
     * @param other the encoder to move from; left empty
     */
    VideoEncoderContext(VideoEncoderContext&& other);

    /**
     * Move assignment.
     *
     * @param other the encoder to move from; left empty
     *
     * @return this encoder
     */
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


#if AVCPP_NEW_CODEC_API

    /**
     * Sends a frame to the encoder.
     *
     * @param frame the frame to encode
     * @param ec    holder for the error code
     *
     * @return false if the encoder cannot accept more input until receive()
     *         is called. This EAGAIN condition is not an error.
     */
    bool send(const VideoFrame& frame, OptionalErrorCode ec = throws());

    /**
     * @brief Receives one encoded packet.
     *
     * Returns an empty packet when no packet is available yet (EAGAIN) and
     * when the stream has ended. Use isEof() to distinguish the two cases.
     *
     * @param ec holder for the error code
     *
     * @return the encoded packet, or an empty packet as described above
     */
    Packet receive(OptionalErrorCode ec = throws());

#endif

    /**
     * @brief Sets AVCodecContext::hw_device_ctx before open().
     * @param device
     * @param ec
     */
    void setHwDeviceContext(const hw::DeviceContext& device,
                            OptionalErrorCode ec = throws());

    /**
     * @brief Removes the hardware device context.
     */
    void clearHwDeviceContext() noexcept;

    /**
     * @brief Sets AVCodecContext::hw_frames_ctx before open().
     *
     * The encoder will expect hardware frames. Use this for the zero-copy path:
     * pass the frames context obtained from a decoded VideoFrame.
     * @param frames
     * @param ec
     */
    void setHwFramesContext(const hw::FramesContext& frames,
                            OptionalErrorCode ec = throws());

    /**
     * @brief Removes the hardware frames context.
     */
    void clearHwFramesContext() noexcept;

    /**
     * Device configured for this encoder.
     *
     * @return the device set by configureHwEncoding() or setHwDeviceContext(),
     *         or an invalid value when encoding on the CPU
     */
    hw::DeviceContext hwDeviceContext() const noexcept;

    /**
     * Frames context configured for this encoder.
     *
     * @return the pool set by configureHwEncoding(const hw::FramesContext&) or
     *         setHwFramesContext(), or an invalid value when there is none
     */
    hw::FramesContext hwFramesContext() const noexcept;

    /**
     * @brief Configures hardware encoding for the given device.
     *
     * @param device    hardware device. It must match the encoder.
     * @param mode      Auto prefers a frames context, Frames requires it,
     *                  Device uses AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX.
     * @param swFormat  software pixel format for a frames context. When
     *                  AV_PIX_FMT_NONE, the current pixel format or the first
     *                  format from FrameConstraints::swFormats() is used.
     *
     * Auto creates a frames context when the codec has a frames configuration.
     * It falls back to a device context when frames creation fails and a device
     * configuration exists. Otherwise it reports HardwareConfigNotFound.
     * @param ec
     */
    void configureHwEncoding(const hw::DeviceContext& device,
                             hw::EncodingMode mode = hw::EncodingMode::Auto,
                             PixelFormat swFormat = AV_PIX_FMT_NONE,
                             OptionalErrorCode ec = throws());

    /**
     * @brief Uses an existing frames context (zero-copy).
     *
     * Sets hw_frames_ctx and the hardware pixel format, and disables automatic
     * upload. The encoder expects hardware frames from this context.
     * @param frames
     * @param ec
     */
    void configureHwEncoding(const hw::FramesContext& frames,
                             OptionalErrorCode ec = throws());

    /**
     * How the encoder receives hardware frames.
     *
     * @return the mode chosen by configureHwEncoding()
     */
    hw::EncodingMode hwEncodingMode() const noexcept;
    /**
     * Whether hardware encoding is configured.
     *
     * @return `true` when a device or a frames context is attached
     */
    bool isHwEncoding() const noexcept;

    /**
     * @brief Upload software frames automatically before encoding.
     *
     * Enabled by default. The function is used only when a hardware frames
     * context is configured and the incoming frame is a software frame.
     * @param enabled
     */
    void setHwAutoUpload(bool enabled) noexcept;
    /**
     * Whether software frames are uploaded automatically.
     *
     * @return the value set by setHwAutoUpload(); enabled by default
     */
    bool isHwAutoUpload() const noexcept;

};


template<typename Clazz, Direction _direction>
/**
 * Audio-specific parameters shared by audio encoders and decoders.
 *
 * Sample rate, channel count, sample format and channel layout, including the
 * bridge between the old bit-mask layout API and the newer AVChannelLayout one.
 */
class AudioCodecContext : public CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_AUDIO>
{
public:
    /// The base class this context derives from.
    using Parent = CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_AUDIO>;
    using Parent::Parent;
    using Parent::isValid;
    using Parent::isOpened;
    using Parent::_log;

    /**
     * Sample rate of the context.
     *
     * @return samples per second, or zero when the context is not valid
     */
    int sampleRate() const noexcept
    {
        return RAW_GET2(isValid(), sample_rate, 0);
    }

    /**
     * Number of audio channels.
     *
     * @return the channel count, or zero when the context is not valid
     */
    int channels() const noexcept
    {
        if (!isValid())
            return 0;
        return codec_context::audio::get_channels(m_raw);
    }

    /**
     * Sample format of the context.
     *
     * @return the format, or `AV_SAMPLE_FMT_NONE` when the context is not valid
     */
    SampleFormat sampleFormat() const noexcept
    {
        return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
    }

    /**
     * Channel layout as a bit mask.
     *
     * @return the `AV_CH_LAYOUT_*` mask, or zero when the context is not valid
     */
    uint64_t channelLayout() const noexcept
    {
        if (!isValid())
            return 0;
        return codec_context::audio::get_channel_layout_mask(m_raw);
    }

#if AVCPP_API_NEW_CHANNEL_LAYOUT
    /**
     * Channel layout as a structured view.
     *
     * Available with the modern FFmpeg channel layout API.
     *
     * @return a non-owning view of the layout, empty when the context is not valid
     */
    ChannelLayoutView channelLayout2() const noexcept
    {
        if (!isValid())
            return ChannelLayoutView{};
        return ChannelLayoutView{m_raw->ch_layout};
    }
#endif

    /**
     * Set the sample rate.
     *
     * When the codec does not support the value, the closest supported rate is
     * chosen and an informational message is logged.
     *
     * @param sampleRate samples per second
     */
    void setSampleRate(int sampleRate) noexcept
    {
        if (!isValid())
            return;
#if AVCPP_CXX_STANDARD >= 20
        int sr = guessValue(sampleRate, make_array_view_until<const int>(codec_context::internal::get_supported_samplerates(m_raw->codec), 0));
#else
        int sr = guessValue(sampleRate, codec_context::internal::get_supported_samplerates(m_raw->codec), EqualComparator<int>(0));
#endif
        if (sr != sampleRate)
        {
            fflog(AV_LOG_INFO, "Guess sample rate %d instead unsupported %d\n", sr, sampleRate);
        }
        if (sr > 0)
            m_raw->sample_rate = sr;
    }

    /**
     * Set the channel count.
     *
     * @param channels number of channels; non-positive values are ignored
     */
    void setChannels(int channels) noexcept
    {
        if (!isValid() || channels <= 0)
            return;
        codec_context::audio::set_channels(m_raw, channels);
    }

    /**
     * Set the sample format.
     *
     * @param sampleFormat the format the codec will be fed with, or asked to
     *                     produce
     */
    void setSampleFormat(SampleFormat sampleFormat) noexcept
    {
        RAW_SET2(isValid(), sample_fmt, sampleFormat);
    }

    /**
     * Set the channel layout as a bit mask.
     *
     * @param layout an `AV_CH_LAYOUT_*` mask; zero is ignored
     */
    void setChannelLayout(uint64_t layout) noexcept
    {
        if (!isValid() || layout == 0)
            return;
        codec_context::audio::set_channel_layout_mask(m_raw, layout);
    }

#if AVCPP_API_NEW_CHANNEL_LAYOUT
    /**
     * Set the channel layout from a structured value.
     *
     * The layout is moved into the context and the argument is released.
     *
     * @param layout a valid layout; invalid values are ignored
     */
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


/**
 * Decodes audio packets into sample buffers.
 *
 * Uses the send/receive API; the legacy decode() overloads remain for one-shot
 * conversions and for code that predates it.
 */
class AudioDecoderContext : public AudioCodecContext<AudioDecoderContext, Direction::Decoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = AudioCodecContext<AudioDecoderContext, Direction::Decoding>;
    using Parent::Parent;

    /**
     * Create an empty decoder; set a codec and open() it later.
     */
    AudioDecoderContext() = default;
    /**
     * Move the decoder.
     *
     * @param other the decoder to move from; left empty
     */
    AudioDecoderContext(AudioDecoderContext&& other);

    /**
     * Move assignment.
     *
     * @param other the decoder to move from; left empty
     *
     * @return this decoder
     */
    AudioDecoderContext& operator=(AudioDecoderContext&& other);

    /**
     * Decode one audio packet.
     *
     * @param inPacket the compressed packet
     * @param ec       holder for the error code
     *
     * @return decoded samples, or an invalid buffer when the packet produced
     *         no complete buffer
     */
    AudioSamples decode(const Packet &inPacket, OptionalErrorCode ec = throws());
    /**
     * Decode one audio packet starting at a byte offset.
     *
     * @param inPacket the compressed packet
     * @param offset   byte offset inside the packet
     * @param ec       holder for the error code
     *
     * @return decoded samples, or an invalid buffer when nothing was produced
     */
    AudioSamples decode(const Packet &inPacket, size_t offset, OptionalErrorCode ec = throws());

#if AVCPP_NEW_CODEC_API

    /**
     * @brief Sends a packet to the decoder.
     *
     * @return false if the decoder cannot accept more input until receive()
     *         is called. This EAGAIN condition is not an error.
     */
    /**
     * Sends a packet to the decoder.
     *
     * @param packet the compressed packet
     * @param ec     holder for the error code
     *
     * @return false if the decoder cannot accept more input until receive()
     *         is called. This EAGAIN condition is not an error.
     */
    bool send(const Packet& packet, OptionalErrorCode ec = throws());

    /**
     * @brief Receives one decoded audio buffer.
     *
     * Returns invalid samples when no buffer is available yet (EAGAIN) and
     * when the stream has ended. Use isEof() to distinguish the two cases.
     * @param ec
     * @return decoded samples, or an invalid buffer when nothing is available
     */
    AudioSamples receive(OptionalErrorCode ec = throws());

#endif
};


/**
 * Encodes audio samples into packets.
 *
 * Set sample rate, channel count and sample format before open(); feed samples
 * that already match the configured layout, or convert them with
 * av::AudioResampler first.
 */
class AudioEncoderContext : public AudioCodecContext<AudioEncoderContext, Direction::Encoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = AudioCodecContext<AudioEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    /**
     * Create an empty encoder; set a codec and open() it later.
     */
    AudioEncoderContext() = default;
    /**
     * Move the encoder.
     *
     * @param other the encoder to move from; left empty
     */
    AudioEncoderContext(AudioEncoderContext&& other);

    /**
     * Move assignment.
     *
     * @param other the encoder to move from; left empty
     *
     * @return this encoder
     */
    AudioEncoderContext& operator=(AudioEncoderContext&& other);

    /**
     * Drain the encoder.
     *
     * Call it repeatedly without input until it returns an empty packet; one
     * call can produce only one packet, so a loop is required.
     *
     * @param ec holder for the error code
     *
     * @return an encoded packet, or an empty packet when the encoder has
     *         nothing left
     */
    Packet encode(OptionalErrorCode ec = throws());

    /**
     * Encode one buffer of samples.
     *
     * @param inSamples samples in the layout configured on the encoder
     * @param ec        holder for the error code
     *
     * @return an encoded packet, or an empty packet when the encoder buffers
     *         the input and produces nothing yet
     */
    Packet encode(const AudioSamples &inSamples, OptionalErrorCode ec = throws());


#if AVCPP_NEW_CODEC_API

    /**
     * @brief Sends audio samples to the encoder.
     *
     * @return false if the encoder cannot accept more input until receive()
     *         is called. This EAGAIN condition is not an error.
     * @param samples
     * @param ec
     */
    bool send(const AudioSamples &samples, OptionalErrorCode ec = throws());

    /**
     * @brief Receives one encoded packet.
     *
     * Returns an empty packet when no packet is available yet (EAGAIN) and
     * when the stream has ended. Use isEof() to distinguish the two cases.
     *
     * @param ec holder for the error code
     *
     * @return the encoded packet, or an empty packet as described above
     */
    Packet receive(OptionalErrorCode ec = throws());

#endif
};


/**
 * @brief Base class for subtitle codec contexts.
 *
 * Subtitle codecs use the legacy avcodec_decode_subtitle2() /
 * avcodec_encode_subtitle() functions. The FFmpeg send/receive API aborts for
 * subtitle codecs, so send(), receive() and flush() are deleted here and the
 * generic CodecContext2 implementations report
 * Errors::CodecUnsupportedOperation.
 *
 * Text subtitle encoders (subrip, webvtt, mov_text, text) require a subtitle
 * header before open(); use setDefaultSubtitleHeader() for a built-in ASS
 * header. Bitmap encoders such as dvdsub do not need one.
 */
template<typename Clazz, Direction _direction>
class SubtitleCodecContext : public CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_SUBTITLE>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = CodecContextBase<Clazz, _direction, AVMEDIA_TYPE_SUBTITLE>;
    using Parent::Parent;

    /**
     * Subtitle header in ASS/SSA format, or an empty view when no header has
     * been set.
     * @return the header text, empty when no header is set
     */
    std::string_view subtitleHeader() const noexcept
    {
        if (!this->m_raw || !this->m_raw->subtitle_header || this->m_raw->subtitle_header_size <= 0)
            return {};

        return { reinterpret_cast<const char*>(this->m_raw->subtitle_header),
                 static_cast<std::size_t>(this->m_raw->subtitle_header_size) };
    }

    /**
     * Sets the subtitle header. Allowed only before open(): the FFmpeg text
     * subtitle encoders parse the header during avcodec_open2().
     * @param header
     * @param ec
     */
    void setSubtitleHeader(std::string_view header, OptionalErrorCode ec = throws())
    {
        clear_if(ec);

        if (!this->isValid()) {
            throws_if(ec, Errors::CodecInvalid);
            return;
        }

        if (this->isOpened()) {
            throws_if(ec, Errors::CodecAlreadyOpened);
            return;
        }

        std::unique_ptr<uint8_t, void(*)(void*)> copy(nullptr, av_free);
        if (!header.empty()) {
            copy = av::mallocz<uint8_t>(header.size() + 1);
            if (!copy) {
                throws_if(ec, Errors::Unallocated);
                return;
            }
            std::memcpy(copy.get(), header.data(), header.size());
        }

        av::freep(this->m_raw->subtitle_header);
        this->m_raw->subtitle_header = copy.release();
        this->m_raw->subtitle_header_size = static_cast<int>(header.size());
    }

    /**
     * Sets the built-in ASS header accepted by every text subtitle encoder
     * provided by FFmpeg.
     * @param ec
     */
    void setDefaultSubtitleHeader(OptionalErrorCode ec = throws())
    {
        this->setSubtitleHeader(subtitle::defaultAssHeader(), ec);
    }

    /**
     * Releases the subtitle header. Allowed only before open().
     */
    void clearSubtitleHeader() noexcept
    {
        if (!this->m_raw || this->isOpened())
            return;

        av::freep(this->m_raw->subtitle_header);
        this->m_raw->subtitle_header_size = 0;
    }

    /**
     * Time base used by the decoder to fill AVSubtitle::pts. When it is not
     * set, decode() falls back to the packet or stream time base.
     * @return the time base used for decoded subtitle timestamps
     */
    Rational pktTimeBase() const noexcept
    {
        return this->m_raw ? Rational(this->m_raw->pkt_timebase) : Rational();
    }

    /**
     * Set the time base used to fill AVSubtitle::pts.
     *
     * @param value time base for decoded subtitle timestamps; when unset, the
     *              packet or stream time base is used
     */
    void setPktTimeBase(const Rational &value) noexcept
    {
        if (this->m_raw)
            this->m_raw->pkt_timebase = value;
    }

    /**
     * Removing the flush is deliberate: subtitle codecs do not support the
     * send/receive API, so there is no draining mode to enter.
     *
     * @param ec unused
     */
    bool flush(OptionalErrorCode ec = throws()) = delete;
};


/**
 * Decodes subtitle packets into subtitle frames.
 *
 * One packet produces at most one subtitle, and a packet can be consumed
 * without producing anything, for example when the decoder buffers partial
 * data. There is no send/receive pair for subtitles.
 */
class SubtitleDecoderContext : public SubtitleCodecContext<SubtitleDecoderContext, Direction::Decoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = SubtitleCodecContext<SubtitleDecoderContext, Direction::Decoding>;
    using Parent::Parent;

    /**
     * Create an empty decoder; set a codec and open() it later.
     */
    SubtitleDecoderContext() = default;
    /**
     * Move the decoder.
     *
     * @param other the decoder to move from; left empty
     */
    SubtitleDecoderContext(SubtitleDecoderContext &&other);
    /**
     * Move assignment.
     *
     * @param other the decoder to move from; left empty
     *
     * @return this decoder
     */
    SubtitleDecoderContext &operator=(SubtitleDecoderContext &&other);

    /**
     * @brief Decodes one subtitle packet.
     *
     * The returned frame owns its rectangles. AVSubtitle has no reference
     * counting, so every call returns a newly allocated frame.
     *
     * The frame pts is in AV_TIME_BASE units. It is filled when pktTimeBase()
     * or the packet/stream time base is available; otherwise pts() reports
     * NoPts.
     *
     * An invalid frame means that the packet did not produce a subtitle yet
     * (for example, a decoder that buffers partial data).
     * @param packet
     * @param ec
     * @return the decoded subtitle, or an invalid frame when the packet produced none
     */
    SubtitleFrame decode(const Packet &packet, OptionalErrorCode ec = throws());

    /**
     * @brief Decodes one subtitle packet starting at the given offset.
     *
     * @param packet       the compressed packet
     * @param offset       byte offset inside the packet to start from
     * @param[out] decodedBytes number of bytes consumed from the packet
     * @param ec           holder for the error code
     *
     * @return the decoded subtitle, or an invalid frame when the packet did not
     *         produce one
     */
    SubtitleFrame decode(const Packet &packet, size_t offset, size_t &decodedBytes,
                         OptionalErrorCode ec = throws());

#if AVCPP_NEW_CODEC_API
    bool send(const Packet &packet, OptionalErrorCode ec = throws()) = delete;
    Packet receive(OptionalErrorCode ec = throws()) = delete;
#endif
};


/**
 * Encodes subtitle frames into packets.
 *
 * Only SUBTITLE_ASS rectangles with a zero display start are accepted, and text
 * encoders need a subtitle header before open().
 */
class SubtitleEncoderContext : public SubtitleCodecContext<SubtitleEncoderContext, Direction::Encoding>
{
public:
    /**
     * /// The base class this context derives from.
     */
    using Parent = SubtitleCodecContext<SubtitleEncoderContext, Direction::Encoding>;
    using Parent::Parent;

    /**
     * Create an empty encoder; set a codec and open() it later.
     */
    SubtitleEncoderContext() = default;
    /**
     * Move the encoder.
     *
     * @param other the encoder to move from; left empty
     */
    SubtitleEncoderContext(SubtitleEncoderContext &&other);
    /**
     * Move assignment.
     *
     * @param other the encoder to move from; left empty
     *
     * @return this encoder
     */
    SubtitleEncoderContext &operator=(SubtitleEncoderContext &&other);

    /**
     * @brief Encodes one subtitle frame.
     *
     * FFmpeg subtitle encoders accept only SUBTITLE_ASS rectangles and require
     * startDisplayTime() to be zero. Text encoders additionally need a
     * subtitle header set before open().
     *
     * An empty packet means that the encoder produced no output.
     * @param frame
     * @param ec
     * @return the encoded packet, or an empty packet when the encoder produced nothing
     */
    Packet encode(const SubtitleFrame &frame, OptionalErrorCode ec = throws());

#if AVCPP_NEW_CODEC_API
    bool send(const SubtitleFrame &frame, OptionalErrorCode ec = throws()) = delete;
    Packet receive(OptionalErrorCode ec = throws()) = delete;
#endif
};


} // namespace av
