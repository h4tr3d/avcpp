#include <iostream>

#include "avutils.h"
#include "avtime.h"
#include "avlog.h"

#include "formatcontext.h"

using namespace std;

namespace {

int custom_io_read(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return -1;
    av::CustomIO *io = static_cast<av::CustomIO*>(opaque);
    return io->read(buf, buf_size);
}

int custom_io_write(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return -1;
    av::CustomIO *io = static_cast<av::CustomIO*>(opaque);
    return io->write(buf, buf_size);
}

int64_t custom_io_seek(void *opaque, int64_t offset, int whence)
{
    if (!opaque)
        return -1;
    av::CustomIO *io = static_cast<av::CustomIO*>(opaque);
    return io->seek(offset, whence);
}

} // anonymous

namespace av {

FormatContext::FormatContext()
{
    m_raw = avformat_alloc_context();
    setupInterruptHandling();
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

    m_raw->oformat = nullptr;
    m_raw->iformat = const_cast<AVInputFormat*>(format.raw());
}

void FormatContext::setFormat(const OutputFormat &format)
{
    if (isOpened())
    {
        cerr << "Can't set format for opened container\n";
        return;
    }

    m_raw->oformat = const_cast<AVOutputFormat*>(format.raw());
    m_raw->iformat = nullptr;
}

InputFormat FormatContext::inputFormat() const
{
    if (m_raw)
        return InputFormat(m_raw->iformat);
    else
        return InputFormat();
}

OutputFormat FormatContext::outputFormat() const
{
    if (m_raw)
        return OutputFormat(m_raw->oformat);
    else
        return OutputFormat();
}

bool FormatContext::isOutput() const
{
    return !outputFormat().isNull();
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
        closeCodecContexts();

        AVIOContext *avio = m_raw->pb;
        if (isOutput())
        {
            OutputFormat fmt = outputFormat();
            if (!(fmt.flags() & AVFMT_NOFILE) && !(m_raw->flags & AVFMT_FLAG_CUSTOM_IO)) {
                avio_close(m_raw->pb);
            }
            avformat_free_context(m_raw);
        }
        else
        {
            avformat_close_input(&m_raw);
        }

        m_raw = avformat_alloc_context();
        m_monitor.reset(new char);
        m_isOpened = false;

        // To prevent free not out custom IO, e.g. setted via raw pointer access
        if (m_customIO) {
            // Close custom IO
            av_freep(&avio->buffer);
            av_freep(&avio);
            m_customIO = false;
        }
    }
}

void FormatContext::dump() const
{
    if (m_raw)
    {
        av_dump_format(m_raw, 0, m_uri.c_str(), !outputFormat().isNull());
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

    return Stream2(m_monitor, m_raw->streams[idx], isOutput() ? Direction::ENCODING : Direction::DECODING);
}

Stream2 FormatContext::addStream(const Codec &codec)
{
    if (!m_raw)
        return Stream2();

    auto rawcodec = codec.raw();
    AVStream *st = nullptr;

    st = avformat_new_stream(m_raw, rawcodec);
    if (!st)
        return Stream2();

    return Stream2(m_monitor, st, Direction::ENCODING);
}

bool FormatContext::openInput(const std::string &uri, InputFormat format)
{
    if (m_isOpened)
        return false;

    if (format.isNull() && m_raw && m_raw->iformat)
        format = InputFormat(m_raw->iformat);

    resetSocketAccess();
    int ret = avformat_open_input(&m_raw, uri.empty() ? nullptr : uri.c_str(), format.raw(), nullptr);
    if (ret < 0)
        return false;

    m_uri      = uri;
    m_isOpened = true;
    queryInputStreams();
    return true;
}

bool FormatContext::openInput(CustomIO *io, size_t internalBufferSize, InputFormat format)
{
    if (isOpened())
        return false;

    bool res = openCustomIO(io, internalBufferSize, false);
    if (!res)
        return false;
    return openInput(string(), format);
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

bool FormatContext::openOutput(const string &uri)
{
    if (!m_raw)
        return false;

    if (isOpened())
        return false;

    OutputFormat format = outputFormat();

    if (format.isNull()) {
        // Guess format
        format = guessOutputFormat(string(), uri);

        if (format.isNull()) {
            ptr_log(AV_LOG_ERROR, "Can't guess output format");
            return false;
        }
        setFormat(format);
    }

    resetSocketAccess();
    if (!(format.flags() & AVFMT_NOFILE)) {
        int stat = avio_open2(&m_raw->pb, uri.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if (stat < 0)
            return false;
    }

    m_uri = uri;
    m_isOpened = true;
    return true;
}

bool FormatContext::openOutput(CustomIO *io, size_t internalBufferSize)
{
    if (isOpened())
        return false;

    if (m_raw) {
        OutputFormat format = outputFormat();
        if (format.isNull()) {
            ptr_log(AV_LOG_ERROR, "You must set output format for use with custom IO\n");
            return false;
        }
    }

    bool res = openCustomIO(io, internalBufferSize, true);
    if (res) {
        m_isOpened = true;
        m_uri.clear();
    }

    return res;
}

bool FormatContext::writeHeader()
{
    if (isOpened() && isOutput()) {
        resetSocketAccess();
        int stat = avformat_write_header(m_raw, nullptr);
        return !!checkPbError(stat);
    }
    return false;
}

ssize_t FormatContext::writePacket(const Packet &pkt, bool interleave)
{
    if (isOpened() && isOutput()) {
        // Make reference to packet
        auto writePkt = pkt;

        if (!pkt.isNull()) {
            auto streamIndex = pkt.getStreamIndex();
            auto st = stream(streamIndex);

            if (st.isNull()) {
                ptr_log(AV_LOG_WARNING, "Required stream does not exists: %d, total=%ld\n", streamIndex, streamsCount());
                return -1;
            }

            // Set packet time base to stream one
            if (st.timeBase() != pkt.getTimeBase()) {
                writePkt.setTimeBase(st.timeBase());
            }

            if (pkt.getPts() == AV_NOPTS_VALUE && pkt.getFakePts() != AV_NOPTS_VALUE)
                writePkt.setPts(writePkt.getFakePts());
        }

        resetSocketAccess();
        int stat = -1;
        if (interleave)
            stat = av_interleaved_write_frame(m_raw, writePkt.raw());
        else
            stat = av_write_frame(m_raw, writePkt.raw());

        return checkPbError(stat);
    }
    return -1;
}

ssize_t FormatContext::writeTrailer()
{
    if (isOpened() && isOutput()) {
        resetSocketAccess();
        auto stat = av_write_trailer(m_raw);
        return checkPbError(stat);
    }
    return -1;
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

ssize_t FormatContext::checkPbError(ssize_t stat)
{
    // WORKAROUND: a lot of format specific writer_packet() functions always return zero code
    // and av_write_frame() in FFMPEG prio 1.0 does not contain follow wrapper
    // so, we can't detect any write error :-(
    // This workaround should fix this problem
    if (stat >= 0 && m_raw->pb && m_raw->pb->error < 0)
        return m_raw->pb->error;
    return stat;
}

bool FormatContext::openCustomIO(CustomIO *io, size_t internalBufferSize, bool isWritable)
{
    if (!m_raw)
        return false;

    if (isOpened())
        return false;

    resetSocketAccess();

    AVIOContext *ctx = nullptr;
    // Note: buffer must be allocated only with av_malloc() and friends
    uint8_t *internalBuffer = (uint8_t*)av_mallocz(internalBufferSize);
    ctx = avio_alloc_context(internalBuffer, internalBufferSize, isWritable, (void*)(io), custom_io_read, custom_io_write, custom_io_seek);

    if (ctx) {
        ctx->seekable = io->seekable();
        m_raw->flags |= AVFMT_FLAG_CUSTOM_IO;
        m_customIO = true;
    }

    m_raw->pb = ctx;
    return ctx;
}

} // namespace av
