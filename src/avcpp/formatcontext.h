#pragma once

/**
 * Container format contexts: opening, demuxing, muxing and seeking.
 *
 * @file
 * @ingroup avformat
 */

#include <memory>
#include <chrono>
#include <functional>
#include <vector>

#include "ffmpeg.h"
#include "format.h"
#include "avutils.h"
#include "stream.h"
#include "packet.h"
#include "codec.h"
#include "dictionary.h"
#include "averror.h"

#if AVCPP_HAS_AVFORMAT

extern "C" {
#include <libavformat/avformat.h>
}

namespace av {

/**
 * Callback that can abort a blocking IO operation.
 *
 * The callback runs periodically while FFmpeg waits on a socket. A non-zero
 * result interrupts the read or write and surfaces as an FFmpeg error, which
 * is how a stalled network source is made cancellable.
 */
using AvioInterruptCb = std::function<int()>;

/**
 * Interface class to customize IO operations with FormatContext
 *
 * It can be used for:
 * - From memory I/O
 * - mmapped files I/O
 * - Specific containers reading (tar, zip files, for example)
 * - Device I/O
 * - Network I/O
 * - Pattern generation
 * - Image output to the display (for the appropriate formats, like rawvideo)
 *
 * CustomIO object is not owned by the av::FormatContext and must be alived across av::FormatContext life.
 *
 */
struct CustomIO
{
    virtual ~CustomIO() {}

    /**
     * Write part of data to the destination.
     *
     * Note, FFmpeg does not consider return value ​​>0 as the number of bytes actually written and assumes that
     *       all data has been written. Be careful. If data can't be fit into destination better return appropriate
     *       AVERROR code.
     *
     * @see avio_write()
     *
     * @param data   block of data to write
     * @param size   size of the data block
     *
     * @return >=0 on success. AVERROR(xxx) for the system errors (errno wrap) or AVERROR_xxx codes.
     */
    virtual int     write(const uint8_t *data, size_t size) 
    {
        static_cast<void>(data);
        static_cast<void>(size);
        return -1; 
    }

    /**
     * Read part of data from the data source
     *
     * Note, if requested more data that exists to read, you should fill buffer as much as possible and return
     * actual count of the readed data. 0 readed bytes is a valid case, but return it only if there is temporary issues
     * with upstream that can be solved quickly. Otherwise return AVERROR_EOF of AVERROR(EBUSY)/AVERROR(EAGAIN).
     *
     * @see avio_read()
     *
     * @param data  buffer to store data
     * @param size  size of the buffer
     *
     * @return count of the actually readed data. AVERROR(xxx) for the system errors (errno wrap) or AVERROR_xxx for the
     *         FFmpeg errors. AVERROR_EOF should be returns when end of file reached.
     */
    virtual int     read(uint8_t *data, size_t size)
    {
        static_cast<void>(data);
        static_cast<void>(size);
        return -1;
    }

    /**
     * Seek in stream.
     *
     * @a whence may support special FFmpeg-ralated values:
     * - AVSEEK_SIZE - return size without actual seeking. If unsupported, seek() may return <0
     * - AVSEEK_FORCE - read official FFmpeg documentation. Just ignore it.
     *
     * @see avio_seek()
     *
     * @param offset   offset, may be less zero, equal zero or greater zero.
     * @param whence   is a one of SEEK_* from stdio.h
     *
     * @return new position or AVERROR
     */
    virtual int64_t     seek(int64_t offset, int whence)
    {
        static_cast<void>(offset);
        static_cast<void>(whence);
        return -1;
    }

    /**
     * Defines supported types of buffer seeking
     *
     * Used to fill AVIOContext::seekable. In any case, seek() work on the byte level.
     *
     * @return combination of AVIO_SEEKABLE_* flags or zero
     */
    virtual int         seekable() const { return 0; }

    /**
     * Name of the I/O. Didn't used for now.
     *
     * @return  null-terminated name of the Custom I/O implementation
     */
    virtual const char* name() const { return ""; }
};

/**
 * Container context: the entry point for demuxing, muxing and seeking.
 *
 * The context owns the streams and the streams own the packets that come out of
 * them. Reading is a loop of readPacket()/writePacket(); the container itself
 * is opened once and closed by the destructor.
 *
 * The context is not copyable: it owns resources and FFmpeg refers to it by
 * pointer, so moving it would invalidate the relationships between the streams,
 * the packets and the IO layer.
 */
class FormatContext : public FFWrapperPtr<AVFormatContext>, public noncopyable
{
#if AVCPP_CXX_STANDARD >= 20
    struct _StreamTransform {
        const FormatContext *_parent{};
        Stream operator()(AVStream *st) const {
            assert(_parent);
            return Stream{_parent->m_monitor, st, _parent->isOutput() ? Direction::Encoding : Direction::Decoding};
        }
    };
    using StreamsView = std::ranges::transform_view<std::span<AVStream*>, _StreamTransform>;
#endif

public:
    FormatContext();
    ~FormatContext();

    /**
     * Abort network operations that block for too long.
     *
     * @param timeout milliseconds without progress before the operation is
     *                interrupted; a negative value disables the check
     */
    void setSocketTimeout(int64_t timeout);
    /**
     * Install a callback that can cancel blocking IO.
     *
     * @param cb the callback; it is copied and called from the IO thread
     */
    void setInterruptCallback(const AvioInterruptCb& cb);

    /**
     * Force the demuxer used by openInput().
     *
     * @param format the input format; an empty one restores probing by name
     */
    void setFormat(const InputFormat& format);
    /**
     * Force the muxer used by openOutput().
     *
     * @param format the output format; must be set before addStream()
     */
    void setFormat(const OutputFormat& format);

    /**
     * The demuxer in use.
     *
     * @return the input format, or an empty one when the context is not an input
     */
    InputFormat  inputFormat() const;
    /**
     * The muxer in use.
     *
     * @return the output format, or an empty one when the context is not an output
     */
    OutputFormat outputFormat() const;

    /**
     * Whether this context writes instead of reads.
     *
     * @return `true` for an output context
     */
    bool isOutput() const;
    /**
     * Whether the container is open.
     *
     * @return `true` between a successful open and close(), or until destruction
     */
    bool isOpened() const;

    /**
     * Discard buffered packets and internal state.
     *
     * Used after seeking, so that data read before the seek does not leak into the
     * stream that follows it.
     */
    void flush();
    /**
     * Close the container and release its resources.
     *
     * Called by the destructor; call it explicitly only to release file handles
     * early.
     */
    void close();
    /**
     * Print the container layout through the FFmpeg log system.
     *
     * Diagnostic only: streams, codecs and formats are written at info level.
     */
    void dump() const;

    //
    // Streams
    //
    /**
     * Number of streams in the container.
     *
     * @return the stream count; zero before the container is opened
     */
    size_t streamsCount() const;
    /**
     * Stream by index.
     *
     * @param idx zero-based index; must be less than streamsCount()
     *
     * @return the stream handle; it shares the context's lifetime
     */
    Stream stream(size_t idx);
    /**
     * Stream by index, reporting an out-of-range index as an error.
     *
     * @param idx zero-based index
     * @param ec  holder for the error code
     *
     * @return the stream handle, or an invalid one when the index is out of range
     */
    Stream stream(size_t idx, OptionalErrorCode ec);
    /**
     * Add a stream for the given codec.
     *
     * @deprecated The codec argument is not used by the FFmpeg API. Use addStream() without a
     *             codec and attach the configured codec context to the returned stream.
     *
     * @param codec  codec the stream is created for
     * @param ec     holder for the error code; when it is not `throws()` an error is reported
     *               through it instead of an exception
     *
     * @return the new stream, or an invalid one on failure
     */
    [[deprecated("Codec is not used by the FFmpeg API. Use addStream() without codec and point configured codec context after")]]
    Stream addStream(const Codec &codec, OptionalErrorCode ec = throws());
    /**
     * Add a stream to an output container.
     *
     * The stream parameters are set afterwards, usually by copying them from an
     * input stream or by attaching a configured encoder context.
     *
     * @param ec holder for the error code
     *
     * @return the new stream, or an invalid one on failure
     */
    Stream addStream(OptionalErrorCode ec = throws());
    /**
     * Add a video stream configured from an encoder context.
     *
     * @param encCtx an opened video encoder; its parameters are copied
     * @param ec     holder for the error code
     *
     * @return the new stream, or an invalid one on failure
     */
    Stream addStream(const class VideoEncoderContext& encCtx, OptionalErrorCode ec = throws());
    /**
     * Add an audio stream configured from an encoder context.
     *
     * @param encCtx an opened audio encoder; its parameters are copied
     * @param ec     holder for the error code
     *
     * @return the new stream, or an invalid one on failure
     */
    Stream addStream(const class AudioEncoderContext& encCtx, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    /**
     * All streams as a range, for range-based for and std algorithms.
     *
     * @param ec holder for the error code
     *
     * @return a lazy view over the streams, valid while the context is alive
     */
    StreamsView streams(OptionalErrorCode ec = throws()) const;
#endif

    //
    // Seeking
    //
    /**
     * Whether the container supports seeking.
     *
     * @return `true` when the IO layer reports seek support
     */
    bool seekable() const noexcept;
    /**
     * Seek to a timestamp in any stream.
     *
     * @param timestamp target position; a default timestamp seeks to the start
     * @param ec        holder for the error code
     */
    void seek(const Timestamp& timestamp, OptionalErrorCode ec = throws());
    /**
     * Seek to a timestamp in a specific stream.
     *
     * @param timestamp   target position in that stream's time base
     * @param streamIndex index of the stream to seek in
     * @param ec          holder for the error code
     */
    void seek(const Timestamp& timestamp, size_t streamIndex, OptionalErrorCode ec = throws());
    /**
     * Seek to a timestamp, choosing between key frames and any frame.
     *
     * @param timestamp target position
     * @param anyFrame  `true` stops at any frame, `false` at a key frame, which is
     *                  what a decoder needs to start from
     * @param ec        holder for the error code
     */
    void seek(const Timestamp& timestamp, bool anyFrame, OptionalErrorCode ec = throws());
    /**
     * Seek to a timestamp in a specific stream, choosing the frame kind.
     *
     * @param timestamp   target position in that stream's time base
     * @param streamIndex index of the stream to seek in
     * @param anyFrame    `true` stops at any frame, `false` at a key frame
     * @param ec          holder for the error code
     */
    void seek(const Timestamp& timestamp, size_t streamIndex, bool anyFrame, OptionalErrorCode ec = throws());

    /**
     * Seek with raw FFmpeg arguments.
     *
     * @param position    byte position in `AV_TIME_BASE` units when
     *                    `streamIndex` is -1, otherwise in the stream time base
     * @param streamIndex stream to seek in, or -1 for the container default
     * @param flags       combination of `AVSEEK_FLAG_*`
     * @param ec          holder for the error code
     */
    void seek(int64_t position, int streamIndex, int flags, OptionalErrorCode ec = throws());

    //
    // Other tools
    //
    /**
     * Start time of the container.
     *
     * @return the first timestamp of the file, or NoPts when it is unknown
     */
    Timestamp startTime() const noexcept;
    /**
     * Duration of the container.
     *
     * @return the duration, or NoPts when the container does not report one
     */
    Timestamp duration() const noexcept;
    /**
     * Shift timestamps so the file starts at zero.
     *
     * @param enable `true` subtracts startTime() from every timestamp handed out
     */
    void substractStartTime(bool enable);

    /**
     * Flags to the user to detect events happening on the file.
     * A combination of AVFMT_EVENT_FLAG_*. Must be cleared by the user.
     * @see AVFormatContext::event_flags
     * @return
     */
    /**
     * Events FFmpeg raised for this container, as a bit mask.
     *
     * @return a combination of `AVFMT_EVENT_FLAG_*`; the caller must clear it
     */
    int eventFlags() const noexcept;
    /**
     * Whether any of the given events happened.
     *
     * @param flags combination of `AVFMT_EVENT_FLAG_*` to test
     *
     * @return `true` when at least one of the flags is set
     */
    bool eventFlags(int flags) const noexcept;
    /**
     * Acknowledge events.
     *
     * @param flags the `AVFMT_EVENT_FLAG_*` bits to clear
     */
    void eventFlagsClear(int flags) noexcept;

    //
    // Input
    //
    /**
     * Open an input and probe its container.
     *
     * Probing reads the beginning of the stream, so the format is known without
     * calling findStreamInfo().
     *
     * @param uri resource name
     * @param ec  holder for the error code
     */
    void openInput(const std::string& uri, OptionalErrorCode ec = throws());
    /**
     * Open an input with demuxer options.
     *
     * @param uri           resource name
     * @param formatOptions demuxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec            holder for the error code
     */
    void openInput(const std::string& uri, Dictionary &formatOptions, OptionalErrorCode ec = throws());
    /**
     * Open an input with demuxer options, taking ownership of the dictionary.
     *
     * @param uri           resource name
     * @param formatOptions demuxer options, moved into the call
     * @param ec            holder for the error code
     */
    void openInput(const std::string& uri, Dictionary &&formatOptions, OptionalErrorCode ec = throws());

    /**
     * Open an input with an explicit demuxer.
     *
     * @param uri    resource name
     * @param format the demuxer to use instead of probing
     * @param ec     holder for the error code
     */
    void openInput(const std::string& uri, InputFormat format, OptionalErrorCode ec = throws());
    /**
     * Open an input with an explicit demuxer and options.
     *
     * @param uri           resource name
     * @param formatOptions demuxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param format        the demuxer to use
     * @param ec            holder for the error code
     */
    void openInput(const std::string& uri, Dictionary &formatOptions, InputFormat format, OptionalErrorCode ec = throws());
    /**
     * Open an input with an explicit demuxer and options, moving the dictionary.
     *
     * @param uri           resource name
     * @param formatOptions demuxer options, moved into the call
     * @param format        the demuxer to use
     * @param ec            holder for the error code
     */
    void openInput(const std::string& uri, Dictionary &&formatOptions, InputFormat format, OptionalErrorCode ec = throws());

        /// Default size of the buffer avcpp allocates around a custom IO object.
    static constexpr size_t CUSTOM_IO_DEFAULT_BUFFER_SIZE = 200000;

    /**
     * Open an input on a custom IO object.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, InputFormat(), ec, internalBufferSize);
    }
    /**
     * Open an input on a custom IO object with demuxer options.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param formatOptions       demuxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, formatOptions, InputFormat(), ec, internalBufferSize);
    }
    /**
     * Open an input on a custom IO object with demuxer options, moving the dictionary.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param formatOptions       demuxer options, moved into the call
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, std::move(formatOptions), InputFormat(), ec, internalBufferSize);
    }

    /**
     * Open an input on a custom IO object with an explicit demuxer.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param format              the demuxer to use instead of probing
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    /**
     * Open an input on a custom IO object with an explicit demuxer and options.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param formatOptions       demuxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param format              the demuxer to use
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    /**
     * Open an input on a custom IO object with an explicit demuxer and options,
     * moving the dictionary.
     *
     * @param io                  the source of the bytes; it must outlive the context
     * @param formatOptions       demuxer options, moved into the call
     * @param format              the demuxer to use
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the IO buffer avcpp allocates
     */
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    /**
     * Read the stream parameters of an opened input.
     *
     * Required before streams carry usable codec parameters: probing fills them
     * only partially.
     *
     * @param ec holder for the error code
     */
    void findStreamInfo(OptionalErrorCode ec = throws());
    /**
     * Read the stream parameters with per-stream options.
     *
     * @param streamsOptions one dictionary per stream; FFmpeg removes the options it recognizes
     * @param ec             holder for the error code
     */
    void findStreamInfo(DictionaryArray &streamsOptions, OptionalErrorCode ec = throws());
    /**
     * Read the stream parameters with per-stream options, moving them in.
     *
     * @param streamsOptions one dictionary per stream, moved into the call
     * @param ec             holder for the error code
     */
    void findStreamInfo(DictionaryArray &&streamsOptions, OptionalErrorCode ec = throws());

    /**
     * Read the next packet from any stream.
     *
     * @param ec holder for the error code
     *
     * @return the packet, or an empty one at end of file; check isNull() rather
     *         than the error code to detect the end
     */
    Packet readPacket(OptionalErrorCode ec = throws());

    //
    // Output
    //
    /**
     * Open an output.
     *
     * The muxer comes from the format set on the context or from the file name.
     *
     * @param uri resource name
     * @param ec  holder for the error code
     */
    void openOutput(const std::string& uri, OptionalErrorCode ec = throws());
    /**
     * Open an output with muxer options.
     *
     * @param uri     resource name
     * @param options muxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec      holder for the error code
     */
    void openOutput(const std::string& uri, Dictionary &options, OptionalErrorCode ec = throws());
    /**
     * Open an output with muxer options, moving the dictionary.
     *
     * @param uri     resource name
     * @param options muxer options, moved into the call
     * @param ec      holder for the error code
     */
    void openOutput(const std::string& uri, Dictionary &&options, OptionalErrorCode ec = throws());

    /**
     * Open an output with an explicit muxer.
     *
     * @param uri    resource name
     * @param format the muxer to use
     * @param ec     holder for the error code
     */
    void openOutput(const std::string& uri, OutputFormat format, OptionalErrorCode ec = throws());
    /**
     * Open an output with an explicit muxer and options.
     *
     * @param uri     resource name
     * @param options muxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param format  the muxer to use
     * @param ec      holder for the error code
     */
    void openOutput(const std::string& uri, Dictionary &options, OutputFormat format, OptionalErrorCode ec = throws());
    /**
     * Open an output with an explicit muxer and options, moving the dictionary.
     *
     * @param uri     resource name
     * @param options muxer options, moved into the call
     * @param format  the muxer to use
     * @param ec      holder for the error code
     */
    void openOutput(const std::string& uri, Dictionary &&options, OutputFormat format, OptionalErrorCode ec = throws());

    /// @{
    /**
     * Open output on a custom IO object.
     *
     * The output format is deduced from the file name, from OutputFormat set on the context,
     * or from the muxer passed explicitly in the overloads below.
     *
     * @param io                  custom IO object that receives the output
     * @param ec                  holder for the error code; when it is not `throws()` an error
     *                            is reported through it instead of an exception
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     */
    void openOutput(CustomIO *io, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    /**
     * Open output on a custom IO object with explicit muxer options.
     *
     * The options are passed to @ref initOutput(), so `writeHeader()` must not be called with
     * the same dictionary.
     *
     * @param io                  custom IO object that receives the output
     * @param formatOptions       muxer options
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     *
     * @return `true` when the format was initialized during the call, `false` when it will be
     *         initialized by writeHeader()
     *
     * @retval true   the format was initialized during the call
     * @retval false  the format will be initialized by writeHeader()
     */
    bool openOutput(CustomIO *io, Dictionary &formatOptions, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    /**
     * Open output on a custom IO object with muxer options, moving the dictionary.
     *
     * @param io                  custom IO object that receives the output
     * @param formatOptions       muxer options, moved into the call
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     *
     * @return `true` when the format was initialized during the call, `false` when it will be
     *         initialized by writeHeader()
     */
    bool openOutput(CustomIO *io, Dictionary &&formatOptions, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    /**
     * Open output on a custom IO object with an explicit output format.
     *
     * @param io                  custom IO object that receives the output
     * @param format              muxer to use instead of deducing it from the name
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     */
    void openOutput(CustomIO *io, OutputFormat format, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    /**
     * Open output on a custom IO object with an explicit output format and muxer options.
     *
     * @param io                  custom IO object that receives the output
     * @param formatOptions       muxer options, passed to @ref initOutput()
     * @param format              muxer to use
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     *
     * @return `true` when the format was initialized during the call, `false` when it will be
     *         initialized by writeHeader()
     *
     * @retval true   the format was initialized during the call
     * @retval false  the format will be initialized by writeHeader()
     */
    bool openOutput(CustomIO *io, Dictionary &formatOptions, OutputFormat format, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    /**
     * Open output on a custom IO object with an explicit format and options, moving the
     * dictionary.
     *
     * @param io                  custom IO object that receives the output
     * @param formatOptions       muxer options, moved into the call
     * @param format              muxer to use
     * @param ec                  holder for the error code
     * @param internalBufferSize  size of the buffer allocated around the custom IO object
     *
     * @return `true` when the format was initialized during the call, `false` when it will be
     *         initialized by writeHeader()
     */
    bool openOutput(CustomIO *io, Dictionary &&formatOptions, OutputFormat format, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    /// @}

    /// @{
    /**
     * Init output without writing the header.
     *
     * All overloads accept the output options as a dictionary, or use the ones set on the
     * context. If a dictionary is passed here, writeHeader() must not be called with the same
     * dictionary.
     *
     * @param ec  holder for the error code; when it is not `throws()` an error is reported
     *            through it instead of an exception
     *
     * @retval true   format initialized in avformat_init_output(), @see AVSTREAM_INIT_IN_INIT_OUTPUT
     * @retval false  format will be initialized in avformat_write_header(), @see AVSTREAM_INIT_IN_WRITE_HEADER
     */
    bool initOutput(OptionalErrorCode ec = throws());
    /**
     * Init the output with muxer options instead of writing the header.
     *
     * @param options muxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec      holder for the error code
     *
     * @return `true` when the format was initialized here, `false` when it will be initialized by
     *         writeHeader()
     */
    bool initOutput(Dictionary &options, OptionalErrorCode ec = throws());
    /**
     * Init the output with muxer options, moving the dictionary.
     *
     * @param options muxer options, moved into the call
     * @param ec      holder for the error code
     *
     * @return `true` when the format was initialized here, `false` when it will be initialized by
     *         writeHeader()
     */
    bool initOutput(Dictionary &&options, OptionalErrorCode ec = throws());
    /// @}

    /**
     * Write the container header.
     *
     * Must be called after the streams are added and the output is opened, and
     * before the first packet is written.
     *
     * @param ec holder for the error code
     */
    void writeHeader(OptionalErrorCode ec = throws());
    /**
     * Write the container header with muxer options.
     *
     * @param options muxer options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec      holder for the error code
     */
    void writeHeader(Dictionary &options, OptionalErrorCode ec = throws());
    /**
     * Write the container header with muxer options, moving the dictionary.
     *
     * @param options muxer options, moved into the call
     * @param ec      holder for the error code
     */
    void writeHeader(Dictionary &&options, OptionalErrorCode ec = throws());

    /**
     * Flush the muxer queue.
     *
     * Call it before writeTrailer() when the muxer may still hold packets, which
     * some formats do for reordering.
     *
     * @param ec holder for the error code
     */
    void writePacket(OptionalErrorCode ec = throws());
    /**
     * Write one packet.
     *
     * The packet must already carry the output stream index and timestamps in that
     * stream's time base.
     *
     * @param pkt the packet to write
     * @param ec  holder for the error code
     */
    void writePacket(const Packet &pkt, OptionalErrorCode ec = throws());
    /**
     * Flush the muxer queue without intercepting FFmpeg errors.
     *
     * @param ec holder for the error code
     */
    void writePacketDirect(OptionalErrorCode ec = throws());
    /**
     * Write one packet without intercepting FFmpeg errors.
     *
     * Used when the caller needs the raw error codes instead of the avcpp
     * categories.
     *
     * @param pkt the packet to write
     * @param ec  holder for the error code
     */
    void writePacketDirect(const Packet &pkt, OptionalErrorCode ec = throws());

    /**
     * Whether an uncompressed frame can be written to a stream.
     *
     * Some muxers accept raw frames instead of encoded packets; this checks the
     * stream configuration before the frame is handed over.
     *
     * @param streamIndex index of the output stream
     * @param ec          set when the check fails
     *
     * @return `true` when writeUncodedFrame() can be used
     */
    bool checkUncodedFrameWriting(size_t streamIndex, std::error_code &ec) noexcept;
    /**
     * Whether an uncompressed frame can be written to a stream.
     *
     * @param streamIndex index of the output stream
     *
     * @return `true` when writeUncodedFrame() can be used
     */
    bool checkUncodedFrameWriting(size_t streamIndex) noexcept;

    /**
     * Write an uncompressed video frame through the muxer.
     *
     * Only formats that accept raw video take this path; check it with
     * checkUncodedFrameWriting() first.
     *
     * @param frame       the frame to write; the muxer may reference it
     * @param streamIndex index of the output stream
     * @param ec          holder for the error code
     */
    void writeUncodedFrame(class VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    /**
     * Write an uncompressed video frame without intercepting FFmpeg errors.
     *
     * @param frame       the frame to write
     * @param streamIndex index of the output stream
     * @param ec          holder for the error code
     */
    void writeUncodedFrameDirect(class VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    /**
     * Write uncompressed audio samples through the muxer.
     *
     * @param frame       the samples to write
     * @param streamIndex index of the output stream
     * @param ec          holder for the error code
     */
    void writeUncodedFrame(class AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    /**
     * Write uncompressed audio samples without intercepting FFmpeg errors.
     *
     * @param frame       the samples to write
     * @param streamIndex index of the output stream
     * @param ec          holder for the error code
     */
    void writeUncodedFrameDirect(class AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec = throws());

    /**
     * Write the container trailer and close the output.
     *
     * Without it a file can be unplayable even though its size looks plausible:
     * the index is written here.
     *
     * @param ec holder for the error code
     */
    void writeTrailer(OptionalErrorCode ec = throws());

private:
    void openInput(const std::string& uri, InputFormat format, AVDictionary **options, OptionalErrorCode ec);
    void openOutput(const std::string& uri, OutputFormat format, AVDictionary **options, OptionalErrorCode ec);
    bool initOutput(Dictionary &options, bool closeOnError, OptionalErrorCode ec);
    bool initOutput(AVDictionary **options, OptionalErrorCode ec);
    void writeHeader(AVDictionary **options, OptionalErrorCode ec);
    void writePacket(const Packet &pkt, OptionalErrorCode ec, int(*write_proc)(AVFormatContext *, AVPacket *));
    void writeFrame(AVFrame *frame, int streamIndex, OptionalErrorCode ec, int(*write_proc)(AVFormatContext*,int,AVFrame*));

    Stream addStream(const class CodecContext2 &ctx, OptionalErrorCode ec);

    static int  avioInterruptCb(void *opaque);
    int         avioInterruptCb();
    void        setupInterruptHandling();
    void        resetSocketAccess();
    void        findStreamInfo(AVDictionary **options, size_t optionsCount, OptionalErrorCode ec);
    void        closeCodecContexts();
    int         checkPbError(int stat);

    void        openCustomIO(CustomIO *io, size_t internalBufferSize, bool isWritable, OptionalErrorCode ec);
    void        openCustomIOInput(CustomIO *io, size_t internalBufferSize, OptionalErrorCode ec);
    void        openCustomIOOutput(CustomIO *io, size_t internalBufferSize, OptionalErrorCode ec);

private:
    std::shared_ptr<char>                              m_monitor {new char};
    std::chrono::time_point<std::chrono::system_clock> m_lastSocketAccess;
    int64_t                                            m_socketTimeout = -1;
    AvioInterruptCb                                    m_interruptCb;

    bool                                               m_isOpened = false;
    bool                                               m_customIO = false;
    bool                                               m_streamsInfoFound = false;
    bool                                               m_headerWriten     = false;
    bool                                               m_substractStartTime = false;
};

} // namespace av

#endif // if AVCPP_HAS_AVFORMAT
