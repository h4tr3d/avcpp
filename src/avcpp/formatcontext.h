#pragma once

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

    void setSocketTimeout(int64_t timeout);
    void setInterruptCallback(const AvioInterruptCb& cb);

    void setFormat(const InputFormat& format);
    void setFormat(const OutputFormat& format);

    InputFormat  inputFormat() const;
    OutputFormat outputFormat() const;

    bool isOutput() const;
    bool isOpened() const;

    void flush();
    void close();
    void dump() const;

    //
    // Streams
    //
    size_t streamsCount() const;
    Stream stream(size_t idx);
    Stream stream(size_t idx, OptionalErrorCode ec);
    [[deprecated("Codec is not used by the FFmpeg API. Use addStream() without codec and point configured codec context after")]]
    Stream addStream(const Codec &codec, OptionalErrorCode ec = throws());
    Stream addStream(OptionalErrorCode ec = throws());
    Stream addStream(const class VideoEncoderContext& encCtx, OptionalErrorCode ec = throws());
    Stream addStream(const class AudioEncoderContext& encCtx, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    StreamsView streams(OptionalErrorCode ec = throws()) const;
#endif

    //
    // Seeking
    //
    bool seekable() const noexcept;
    void seek(const Timestamp& timestamp, OptionalErrorCode ec = throws());
    void seek(const Timestamp& timestamp, size_t streamIndex, OptionalErrorCode ec = throws());
    void seek(const Timestamp& timestamp, bool anyFrame, OptionalErrorCode ec = throws());
    void seek(const Timestamp& timestamp, size_t streamIndex, bool anyFrame, OptionalErrorCode ec = throws());

    void seek(int64_t position, int streamIndex, int flags, OptionalErrorCode ec = throws());

    //
    // Other tools
    //
    Timestamp startTime() const noexcept;
    Timestamp duration() const noexcept;
    void substractStartTime(bool enable);

    /**
     * Flags to the user to detect events happening on the file.
     * A combination of AVFMT_EVENT_FLAG_*. Must be cleared by the user.
     * @see AVFormatContext::event_flags
     * @return
     */
    int eventFlags() const noexcept;
    bool eventFlags(int flags) const noexcept;
    void eventFlagsClear(int flags) noexcept;

    //
    // Input
    //
    void openInput(const std::string& uri, OptionalErrorCode ec = throws());
    void openInput(const std::string& uri, Dictionary &formatOptions, OptionalErrorCode ec = throws());
    void openInput(const std::string& uri, Dictionary &&formatOptions, OptionalErrorCode ec = throws());

    void openInput(const std::string& uri, InputFormat format, OptionalErrorCode ec = throws());
    void openInput(const std::string& uri, Dictionary &formatOptions, InputFormat format, OptionalErrorCode ec = throws());
    void openInput(const std::string& uri, Dictionary &&formatOptions, InputFormat format, OptionalErrorCode ec = throws());

    static constexpr size_t CUSTOM_IO_DEFAULT_BUFFER_SIZE = 200000;

    void openInput(CustomIO    *io,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, InputFormat(), ec, internalBufferSize);
    }
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, formatOptions, InputFormat(), ec, internalBufferSize);
    }
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE)
    {
        return openInput(io, std::move(formatOptions), InputFormat(), ec, internalBufferSize);
    }

    void openInput(CustomIO    *io,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   InputFormat  format,
                   OptionalErrorCode ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    void findStreamInfo(OptionalErrorCode ec = throws());
    void findStreamInfo(DictionaryArray &streamsOptions, OptionalErrorCode ec = throws());
    void findStreamInfo(DictionaryArray &&streamsOptions, OptionalErrorCode ec = throws());

    Packet readPacket(OptionalErrorCode ec = throws());

    //
    // Output
    //
    void openOutput(const std::string& uri, OptionalErrorCode ec = throws());
    void openOutput(const std::string& uri, Dictionary &options, OptionalErrorCode ec = throws());
    void openOutput(const std::string& uri, Dictionary &&options, OptionalErrorCode ec = throws());

    // TBD
    //void openOutput(const std::string& uri, OutputFormat format, OptionalErrorCode ec = throws());
    //void openOutput(const std::string& uri, Dictionary &options, OutputFormat format, OptionalErrorCode ec = throws());
    //void openOutput(const std::string& uri, Dictionary &&options, OutputFormat format, OptionalErrorCode ec = throws());

    void openOutput(CustomIO *io, OptionalErrorCode ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    void writeHeader(OptionalErrorCode ec = throws());
    void writeHeader(Dictionary &options, OptionalErrorCode ec = throws());
    void writeHeader(Dictionary &&options, OptionalErrorCode ec = throws());

    void writePacket(OptionalErrorCode ec = throws());
    void writePacket(const Packet &pkt, OptionalErrorCode ec = throws());
    void writePacketDirect(OptionalErrorCode ec = throws());
    void writePacketDirect(const Packet &pkt, OptionalErrorCode ec = throws());

    bool checkUncodedFrameWriting(size_t streamIndex, std::error_code &ec) noexcept;
    bool checkUncodedFrameWriting(size_t streamIndex) noexcept;

    void writeUncodedFrame(class VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    void writeUncodedFrameDirect(class VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    void writeUncodedFrame(class AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec = throws());
    void writeUncodedFrameDirect(class AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec = throws());

    void writeTrailer(OptionalErrorCode ec = throws());

private:
    void openInput(const std::string& uri, InputFormat format, AVDictionary **options, OptionalErrorCode ec);
    void openOutput(const std::string& uri, OutputFormat format, AVDictionary **options, OptionalErrorCode ec);
    void writeHeader(AVDictionary **options, OptionalErrorCode ec = throws());
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