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

namespace av {

using AvioInterruptCb = std::function<int()>;

using FormatContextPtr  = std::shared_ptr<class FormatContext>;
using FormatContextWPtr = std::weak_ptr<class FormatContext>;

class FormatContext :
        public FFWrapperPtr<AVFormatContext>
{
public:
    FormatContext();
    ~FormatContext();

    // Disable copy/move
    FormatContext(const FormatContext&) = delete;
    void operator=(const FormatContext&) = delete;

    void setSocketTimeout(int64_t timeout);
    void setInterruptCallback(const AvioInterruptCb& cb);

    void setFormat(const InputFormat& format);
    void setFormat(const OutputFormat& format);

    const InputFormat& inputFormat() const;
    const OutputFormat& outputFormat() const;

    bool isOutput() const;
    bool isOpened() const;

    void flush();
    void close();
    void dump() const;

    size_t streamsCount() const;
    Stream2 stream(size_t idx);

    bool openInput(const std::string& uri, InputFormat format = InputFormat());

    ssize_t readPacket(Packet &pkt);

private:
    static int  avioInterruptCb(void *opaque);
    int         avioInterruptCb();
    void        setupInterruptHandling();
    void        resetSocketAccess();

    void        setFormat();

    void        queryInputStreams();

    void        closeCodecContexts();

private:
    std::shared_ptr<char>                              m_monitor {new char};
    std::chrono::time_point<std::chrono::system_clock> m_lastSocketAccess;
    int64_t                                            m_socketTimeout = -1;
    AvioInterruptCb                                    m_interruptCb;

    InputFormat                                        m_iformat;
    OutputFormat                                       m_oformat;

    bool                                               m_isOpened = false;
    std::string                                        m_uri;
};

} // namespace av

#endif // AV_FORMATCONTEXT_H
