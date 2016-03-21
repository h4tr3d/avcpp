#ifndef AV_FORMATCONTEXT_H
#define AV_FORMATCONTEXT_H

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

extern "C" {
#include <libavformat/avformat.h>
//#include <libavformat/avio.h>
}

namespace av {

using AvioInterruptCb = std::function<int()>;

struct CustomIO
{
    virtual ~CustomIO() {}
    virtual ssize_t     write(const uint8_t *data, size_t size) 
    {
        static_cast<void>(data);
        static_cast<void>(size);
        return -1; 
    }
    virtual ssize_t     read(uint8_t *data, size_t size)
    {
        static_cast<void>(data);
        static_cast<void>(size);
        return -1;
    }
    /// whence is a one of SEEK_* from stdio.h
    virtual int64_t     seek(int64_t offset, int whence)
    {
        static_cast<void>(offset);
        static_cast<void>(whence);
        return -1;
    }
    /// Return combination of AVIO_SEEKABLE_* flags or zero
    virtual int         seekable() const { return 0; }
    virtual const char* name() const { return ""; }
};

class FormatContext : public FFWrapperPtr<AVFormatContext>, public noncopyable
{
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
    size_t  streamsCount() const;
    Stream2 stream(size_t idx);
    Stream2 stream(size_t idx, std::error_code &ec);
    Stream2 addStream(const Codec &codec, std::error_code &ec = throws());

    //
    // Seeking
    //
    bool seekable() const noexcept;
    void seek(const Timestamp& timestamp, std::error_code &ec = throws());
    void seek(const Timestamp& timestamp, size_t streamIndex, std::error_code &ec = throws());
    void seek(const Timestamp& timestamp, bool anyFrame, std::error_code &ec = throws());
    void seek(const Timestamp& timestamp, size_t streamIndex, bool anyFrame, std::error_code &ec = throws());

    void seek(int64_t position, int streamIndex, int flags, std::error_code &ec = throws());

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
    void openInput(const std::string& uri, std::error_code &ec = throws());
    void openInput(const std::string& uri, Dictionary &formatOptions, std::error_code &ec = throws());
    void openInput(const std::string& uri, Dictionary &&formatOptions, std::error_code &ec = throws());

    void openInput(const std::string& uri, InputFormat format, std::error_code &ec = throws());
    void openInput(const std::string& uri, Dictionary &formatOptions, InputFormat format, std::error_code &ec = throws());
    void openInput(const std::string& uri, Dictionary &&formatOptions, InputFormat format, std::error_code &ec = throws());

    static const size_t CUSTOM_IO_DEFAULT_BUFFER_SIZE = 200000;

    void openInput(CustomIO    *io,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    void openInput(CustomIO    *io,
                   InputFormat  format,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   InputFormat  format,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);
    void openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   InputFormat  format,
                   std::error_code &ec = throws(),
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    void findStreamInfo(std::error_code &ec = throws());
    void findStreamInfo(DictionaryArray &streamsOptions, std::error_code &ec = throws());
    void findStreamInfo(DictionaryArray &&streamsOptions, std::error_code &ec = throws());

    Packet readPacket(std::error_code &ec = throws());

    //
    // Output
    //
    void openOutput(const std::string& uri, std::error_code &ec = throws());
    void openOutput(const std::string& uri, Dictionary &options, std::error_code &ec = throws());
    void openOutput(const std::string& uri, Dictionary &&options, std::error_code &ec = throws());

    // TBD
    //void openOutput(const std::string& uri, OutputFormat format, std::error_code &ec = throws());
    //void openOutput(const std::string& uri, Dictionary &options, OutputFormat format, std::error_code &ec = throws());
    //void openOutput(const std::string& uri, Dictionary &&options, OutputFormat format, std::error_code &ec = throws());

    void openOutput(CustomIO *io, std::error_code &ec = throws(), size_t internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE);

    void writeHeader(std::error_code &ec = throws());
    void writeHeader(Dictionary &options, std::error_code &ec = throws());
    void writeHeader(Dictionary &&options, std::error_code &ec = throws());

    void writePacket(std::error_code &ec = throws());
    void writePacket(const Packet &pkt, std::error_code &ec = throws());
    void writePacketDirect(std::error_code &ec = throws());
    void writePacketDirect(const Packet &pkt, std::error_code &ec = throws());

    bool checkUncodedFrameWriting(size_t streamIndex, std::error_code &ec) noexcept;
    bool checkUncodedFrameWriting(size_t streamIndex) noexcept;

    void writeUncodedFrame(VideoFrame2 &frame, size_t streamIndex, std::error_code &ec = throws());
    void writeUncodedFrameDirect(VideoFrame2 &frame, size_t streamIndex, std::error_code &ec = throws());
    void writeUncodedFrame(AudioSamples2 &frame, size_t streamIndex, std::error_code &ec = throws());
    void writeUncodedFrameDirect(AudioSamples2 &frame, size_t streamIndex, std::error_code &ec = throws());

    void writeTrailer(std::error_code &ec = throws());

private:
    void openInput(const std::string& uri, InputFormat format, AVDictionary **options, std::error_code &ec);
    void openOutput(const std::string& uri, OutputFormat format, AVDictionary **options, std::error_code &ec);
    void writeHeader(AVDictionary **options, std::error_code &ec = throws());
    void writePacket(const Packet &pkt, std::error_code &ec, int(*write_proc)(AVFormatContext *, AVPacket *));
    void writeFrame(AVFrame *frame, int streamIndex, std::error_code &ec, int(*write_proc)(AVFormatContext*,int,AVFrame*));


    static int  avioInterruptCb(void *opaque);
    int         avioInterruptCb();
    void        setupInterruptHandling();
    void        resetSocketAccess();
    void        findStreamInfo(AVDictionary **options, size_t optionsCount, std::error_code &ec);
    void        closeCodecContexts();
    ssize_t     checkPbError(ssize_t stat);

    void        openCustomIO(CustomIO *io, size_t internalBufferSize, bool isWritable, std::error_code &ec);
    void        openCustomIOInput(CustomIO *io, size_t internalBufferSize, std::error_code &ec);
    void        openCustomIOOutput(CustomIO *io, size_t internalBufferSize, std::error_code &ec);

private:
    std::shared_ptr<char>                              m_monitor {new char};
    std::chrono::time_point<std::chrono::system_clock> m_lastSocketAccess;
    int64_t                                            m_socketTimeout = -1;
    AvioInterruptCb                                    m_interruptCb;

    bool                                               m_isOpened = false;
    bool                                               m_customIO = false;
    std::string                                        m_uri;
    bool                                               m_streamsInfoFound = false;
    bool                                               m_headerWriten     = false;
    bool                                               m_substractStartTime = false;
};

} // namespace av

#endif // AV_FORMATCONTEXT_H
