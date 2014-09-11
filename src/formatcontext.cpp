#include <iostream>

#include "avutils.h"
#include "avtime.h"

#include "formatcontext.h"

using namespace std;

namespace av {

FormatContext::FormatContext()
{
    m_raw = avformat_alloc_context();
}

FormatContext::~FormatContext()
{
    close();
    avformat_free_context(m_raw);
}

void FormatContext::setSocketTimeout(int64_t timeout)
{
    m_socketTimeout = timeout;
}

void FormatContext::setInterruptCallback(const AvioInterruptCb &cb)
{
    m_interruptCb = cb;
}

void FormatContext::setFormat(const InputFormat &format)
{
    if (isOpened())
    {
        cerr << "Can't set format for opened container\n";
        return;
    }

    m_iformat = format;
    m_oformat.reset();
    setFormat();
}

void FormatContext::setFormat(const OutputFormat &format)
{
    if (isOpened())
    {
        cerr << "Can't set format for opened container\n";
        return;
    }

    m_iformat.reset();
    m_oformat = format;
    setFormat();
}

const InputFormat &FormatContext::inputFormat() const
{
    return m_iformat;
}

const OutputFormat &FormatContext::outputFormat() const
{
    return m_oformat;
}

bool FormatContext::isOutput() const
{
    return !m_oformat.isNull();
}

bool FormatContext::isOpened() const
{
    return m_isOpened;
}

void FormatContext::flush()
{
    if (isOpened() && m_raw->pb)
    {
        avio_flush(m_raw->pb);
    }
}

void FormatContext::close()
{
    if (!m_raw)
        return;

    if (isOpened())
    {
        if (isOutput())
        {
            //
        }
        else
        {
            closeCodecContexts();
            avformat_close_input(&m_raw);
            m_raw = avformat_alloc_context();
            m_monitor.reset(new char);
        }
        m_isOpened = false;
    }
}

void FormatContext::dump() const
{
    if (m_raw)
    {
        av_dump_format(m_raw, 0, m_uri.c_str(), !m_oformat.isNull());
    }
}

size_t FormatContext::streamsCount() const
{
    return RAW_GET(nb_streams, 0);
}

Stream2 FormatContext::stream(size_t idx)
{
    if (!m_raw || idx >= m_raw->nb_streams)
        return Stream2(m_monitor);

    return Stream2(m_monitor, m_raw->streams[idx], isOutput() ? ENCODING : DECODING);
}

bool FormatContext::openInput(const std::string &uri, InputFormat format)
{
    if (m_isOpened)
        return false;

    resetSocketAccess();
    int ret = avformat_open_input(&m_raw, uri.c_str(), format.raw(), nullptr);
    if (ret < 0)
        return false;

    m_uri      = uri;
    m_isOpened = true;
    queryInputStreams();
    return true;
}

ssize_t FormatContext::readPacket(Packet &pkt)
{
    if (!m_raw)
        return -1;

    Packet packet;

    int stat = 0;
    int tries = 0;
    const int retryCount = 5;
    do
    {
        resetSocketAccess();
        stat = av_read_frame(m_raw, packet.getAVPacket());
        ++tries;
    }
    while (stat == AVERROR(EAGAIN) && (retryCount < 0 || tries <= retryCount));

    pkt = std::move(packet);

    if (pkt.getStreamIndex() >= 0)
        pkt.setTimeBase(m_raw->streams[pkt.getStreamIndex()]->time_base);

    return stat;
}

int FormatContext::avioInterruptCb(void *opaque)
{
    if (!opaque)
        return 0;
    FormatContext *ctx = static_cast<FormatContext*>(opaque);
    return ctx->avioInterruptCb();
}

int FormatContext::avioInterruptCb()
{
    if (m_interruptCb && m_interruptCb())
        return 1;

    // check socket timeout
    std::chrono::time_point<std::chrono::system_clock> currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsedTime = currentTime - m_lastSocketAccess;
    if (elapsedTime.count() > m_socketTimeout && m_socketTimeout > -1)
    {
        std::cerr << "Socket timeout" << std::endl;
        return 1;
    }

    return 0;
}

void FormatContext::setupInterruptHandling()
{
    // Set up thread interrupt handling
    m_raw->interrupt_callback.callback = FormatContext::avioInterruptCb;
    m_raw->interrupt_callback.opaque   = this;
}

void FormatContext::resetSocketAccess()
{
    m_lastSocketAccess = std::chrono::system_clock::now();
}

void FormatContext::setFormat()
{
    m_raw->oformat = m_oformat.raw();
    m_raw->iformat = m_iformat.raw();
}

void FormatContext::queryInputStreams()
{
    // Temporary disable socket timeout
    ScopedValue<int64_t> scopedTimeoutDisable(m_socketTimeout, -1, m_socketTimeout);

    int stat = avformat_find_stream_info(m_raw, nullptr);
    if (stat >= 0 && m_raw->nb_streams > 0)
    {
        av_dump_format(m_raw, 0, m_uri.c_str(), 0);
    }
    else
        cerr << "Could not found streams in input container\n";
}

void FormatContext::closeCodecContexts()
{
    // HACK: This is hack to correct cleanup codec contexts in independ way
    auto nb = m_raw->nb_streams;
    for (auto i = 0; i < nb; ++i) {
        auto st = m_raw->streams[i];
        auto ctx = st->codec;
        avcodec_close(ctx);
    }
}

} // namespace av
