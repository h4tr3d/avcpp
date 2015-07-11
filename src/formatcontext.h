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

    // Streams
    size_t  streamsCount() const;
    Stream2 stream(size_t idx);
    Stream2 addStream(const Codec &codec);

    //
    // Input
    //
    bool openInput(const std::string& uri, InputFormat format = InputFormat());
    bool openInput(const std::string& uri, Dictionary &formatOptions, InputFormat format = InputFormat());
    bool openInput(const std::string& uri, Dictionary &&formatOptions, InputFormat format = InputFormat());

    static const size_t CUSTOM_IO_DEFAULT_BUFFER_SIZE = 200000;

    bool openInput(CustomIO    *io,
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE,
                   InputFormat  format             = InputFormat());
    bool openInput(CustomIO    *io,
                   Dictionary  &formatOptions,
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE,
                   InputFormat  format             = InputFormat());
    bool openInput(CustomIO    *io,
                   Dictionary &&formatOptions,
                   size_t       internalBufferSize = CUSTOM_IO_DEFAULT_BUFFER_SIZE,
                   InputFormat  format             = InputFormat());

    bool findStreamInfo();
    bool findStreamInfo(DictionaryArray &streamsOptions);
    bool findStreamInfo(DictionaryArray &&streamsOptions);

    ssize_t readPacket(Packet &pkt);


    // Output
    bool openOutput(const std::string& uri);
    bool openOutput(const std::string& uri, Dictionary &options);
    bool openOutput(const std::string& uri, Dictionary &&options);

    bool openOutput(CustomIO *io, size_t internalBufferSize = 200000);

    bool    writeHeader();
    bool    writeHeader(Dictionary &options);
    bool    writeHeader(Dictionary &&options);

    ssize_t writePacket(const Packet &pkt = Packet(), bool interleave = true);
    ssize_t writeTrailer();

private:
    bool openInput(const std::string& uri, InputFormat format, AVDictionary **options);
    bool openOutput(const std::string& uri, AVDictionary **options);
    bool writeHeader(AVDictionary **options);

    static int  avioInterruptCb(void *opaque);
    int         avioInterruptCb();
    void        setupInterruptHandling();
    void        resetSocketAccess();
    bool        findStreamInfo(AVDictionary **options);
    void        closeCodecContexts();
    ssize_t     checkPbError(ssize_t stat);

    bool        openCustomIO(CustomIO *io, size_t internalBufferSize, bool isWritable);
    bool        openCustomIOInput(CustomIO *io, size_t internalBufferSize);
    bool        openCustomIOOutput(CustomIO *io, size_t internalBufferSize);

private:
    std::shared_ptr<char>                              m_monitor {new char};
    std::chrono::time_point<std::chrono::system_clock> m_lastSocketAccess;
    int64_t                                            m_socketTimeout = -1;
    AvioInterruptCb                                    m_interruptCb;

    bool                                               m_isOpened = false;
    bool                                               m_customIO = false;
    std::string                                        m_uri;
    bool                                               m_streamsInfoFound = false;
};

} // namespace av

#endif // AV_FORMATCONTEXT_H
