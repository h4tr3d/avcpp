#include <iostream>

#include "avutils.h"
#include "avtime.h"
#include "frame.h"
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
    if (isOpened())
        close();
    else if (m_raw)
        closeCodecContexts();
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
        m_streamsInfoFound = false;
        m_headerWriten = false;

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

Stream FormatContext::stream(size_t idx)
{
    if (!m_raw || idx >= m_raw->nb_streams)
        return Stream(m_monitor);

    return Stream(m_monitor, m_raw->streams[idx], isOutput() ? Direction::Encoding : Direction::Decoding);
}

Stream FormatContext::stream(size_t idx, OptionalErrorCode ec)
{
    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return Stream(m_monitor);
    }

    if (idx >= m_raw->nb_streams) {
        throws_if(ec, Errors::FormatInvalidStreamIndex);
        return Stream(m_monitor);
    }

    return stream(idx);
}

Stream FormatContext::addStream(const Codec &codec, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!m_raw)
    {
        throws_if(ec, Errors::Unallocated);
        return Stream();
    }

    auto rawcodec = codec.raw();
    AVStream *st = nullptr;

    st = avformat_new_stream(m_raw, rawcodec);
    if (!st)
    {
        throws_if(ec, Errors::FormatCantAddStream);
        return Stream();
    }

    auto stream = Stream(m_monitor, st, Direction::Encoding);

#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    if (st->codec) {
        if (outputFormat().isFlags(AVFMT_GLOBALHEADER)) {
            st->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    FF_ENABLE_DEPRECATION_WARNINGS
#endif

    return stream;
}

bool FormatContext::seekable() const noexcept
{
    if (m_raw && m_raw->pb) {
        return m_raw->pb->seekable;
    }
    return false;
}

void FormatContext::seek(const Timestamp &timestamp, OptionalErrorCode ec)
{
    seek(timestamp.timestamp(AV_TIME_BASE_Q_CPP), -1, 0, ec);
}

void FormatContext::seek(const Timestamp& timestamp, size_t streamIndex, OptionalErrorCode ec)
{
    auto st = stream(streamIndex, ec);
    if (st.isValid())
        seek(timestamp.timestamp(st.timeBase()), static_cast<int>(streamIndex), 0, ec);
}

void FormatContext::seek(const Timestamp& timestamp, bool anyFrame, OptionalErrorCode ec)
{
    seek(timestamp.timestamp(AV_TIME_BASE_Q_CPP), -1, anyFrame ? AVSEEK_FLAG_ANY : 0, ec);
}

void FormatContext::seek(const Timestamp &timestamp, size_t streamIndex, bool anyFrame, OptionalErrorCode ec)
{
    auto st = stream(streamIndex);
    if (st.isValid())
        seek(timestamp.timestamp(st.timeBase()), static_cast<int>(streamIndex), anyFrame ? AVSEEK_FLAG_ANY : 0, ec);
}

void FormatContext::seek(int64_t position, int streamIndex, int flags, OptionalErrorCode ec)
{
    clear_if(ec);
    const auto sts = av_seek_frame(m_raw, streamIndex, position, flags);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
}

Timestamp FormatContext::startTime() const noexcept
{
    if (isOutput()) {
        return {};
    }

    return {m_raw->start_time, AV_TIME_BASE_Q_CPP};
}

int FormatContext::eventFlags() const noexcept
{
    if (isOutput())
        return 0;
    return m_raw->event_flags;
}

bool FormatContext::eventFlags(int flags) const noexcept
{
    if (isOutput())
        return false;
    return m_raw->event_flags & flags;
}

void FormatContext::eventFlagsClear(int flags) noexcept
{
    if (isOutput())
        return;
    m_raw->event_flags &= ~flags;
}

void FormatContext::substractStartTime(bool enable)
{
    m_substractStartTime = enable;
}


Timestamp FormatContext::duration() const noexcept
{
    if (isOutput()) {
        return {};
    }

    // Taken from the av_dump_format()
    int64_t duration = m_raw->duration;
    if (m_raw->duration != av::NoPts) {
        duration += (m_raw->duration <= INT64_MAX - 5000 ? 5000 : 0);
    }

    return {duration, av::TimeBaseQ};
};

void FormatContext::openInput(const string &uri, OptionalErrorCode ec)
{
    openInput(uri, InputFormat(), ec);
}

void FormatContext::openInput(const string &uri, Dictionary &formatOptions, OptionalErrorCode ec)
{
    openInput(uri, formatOptions, InputFormat(), ec);
}

void FormatContext::openInput(const string &uri, Dictionary &&formatOptions, OptionalErrorCode ec)
{
    openInput(uri, std::move(formatOptions), InputFormat(), ec);
}

void FormatContext::openInput(const string &uri, InputFormat format, OptionalErrorCode ec)
{
    openInput(uri, format, nullptr, ec);
}

void FormatContext::openInput(const string &uri, Dictionary &formatOptions, InputFormat format, OptionalErrorCode ec)
{
    auto dictptr = formatOptions.release();

    ScopeOutAction onReturn([&dictptr, &formatOptions](){
        formatOptions.assign(dictptr);
    });

    openInput(uri, format, &dictptr, ec);
}

void FormatContext::openInput(const string &uri, Dictionary &&formatOptions, InputFormat format, OptionalErrorCode ec)
{
    // It calls non-movable vesion
    return openInput(uri, formatOptions, format, ec);
}

void FormatContext::openInput(const std::string &uri, InputFormat format, AVDictionary **options, OptionalErrorCode ec)
{
    clear_if(ec);

    if (m_isOpened)
    {
        throws_if(ec, Errors::FormatAlreadyOpened);
        return;
    }

    if (format.isNull() && m_raw && m_raw->iformat)
        format = InputFormat(m_raw->iformat);

    resetSocketAccess();
    int ret = avformat_open_input(&m_raw, uri.empty() ? nullptr : uri.c_str(), format.raw(), options);
    if (ret < 0)
    {
        throws_if(ec, ret, ffmpeg_category());
        return;
    }

    m_uri      = uri;
    m_isOpened = true;
}

void FormatContext::openInput(CustomIO       *io,
                              InputFormat     format,
                              OptionalErrorCode ec,
                              size_t           internalBufferSize)
{
    openCustomIOInput(io, internalBufferSize, ec);
    if (!is_error(ec))
        openInput(string(), format, ec);
}

void FormatContext::openInput(CustomIO        *io,
                              Dictionary      &formatOptions,
                              InputFormat      format,
                              OptionalErrorCode ec,
                              size_t           internalBufferSize)
{
    openCustomIOInput(io, internalBufferSize, ec);
    if (!is_error(ec))
        openInput(string(), formatOptions, format, ec);
}

void FormatContext::openInput(CustomIO         *io,
                              Dictionary      &&formatOptions,
                              InputFormat       format,
                              OptionalErrorCode ec,
                              size_t            internalBufferSize)
{
    openInput(io, formatOptions, format, ec, internalBufferSize);
}

void FormatContext::findStreamInfo(OptionalErrorCode ec)
{
    findStreamInfo(nullptr, 0, ec);
}

void FormatContext::findStreamInfo(DictionaryArray &streamsOptions, OptionalErrorCode ec)
{
    auto ptrs = streamsOptions.release();
    auto count = streamsOptions.size();

    ScopeOutAction onReturn([&ptrs, count, &streamsOptions](){
        streamsOptions.assign(ptrs, count);
    });

    findStreamInfo(ptrs, count, ec);
}

void FormatContext::findStreamInfo(DictionaryArray &&streamsOptions, OptionalErrorCode ec)
{
    findStreamInfo(streamsOptions, ec);
}

Packet FormatContext::readPacket(OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw)
    {
        throws_if(ec, Errors::Unallocated);
        return Packet();
    }

    if (!m_streamsInfoFound)
    {
        fflog(AV_LOG_ERROR, "Streams does not found. Try call findStreamInfo()\n");
        throws_if(ec, Errors::FormatNoStreams);
        return Packet();
    }

    Packet packet;

    int sts = 0;
    int tries = 0;
    const int retryCount = 5;
    do
    {
        resetSocketAccess();
        sts = av_read_frame(m_raw, packet.raw());
        ++tries;
    }
    while (sts == AVERROR(EAGAIN) && (retryCount < 0 || tries <= retryCount));

    // End of file
    if (sts == AVERROR_EOF /*|| avio_feof(m_raw->pb)*/) {
        auto ec_tmp = std::error_code(sts, ffmpeg_category());
        fflog(AV_LOG_DEBUG,
              "EOF reaches, error=%d, %s, isNull: %d, stream_index: %d, payload: %p\n",
              sts,
              ec_tmp.message().c_str(),
              packet.isNull(),
              packet.streamIndex(),
              packet.data());
        av_pkt_dump_log2(m_raw, AV_LOG_DEBUG, packet.raw(), 0, m_raw->streams[packet.streamIndex()]);
        if (packet)
            sts = 0; // not an error
        else
            return packet;
    }

    if (sts == 0)
    {
        auto pberr = m_raw->pb ? m_raw->pb->error : 0;
        if (pberr)
        {
            // TODO: need verification
            throws_if(ec, pberr, ffmpeg_category());
            return packet;
        }
    }
    else
    {
        throws_if(ec, sts, ffmpeg_category());
        return packet;
    }

    if (packet.streamIndex() >= 0)
    {
        if ((size_t)packet.streamIndex() > streamsCount())
        {
            throws_if(ec, Errors::FormatInvalidStreamIndex);
            return packet;
        }

        packet.setTimeBase(m_raw->streams[packet.streamIndex()]->time_base);

        if (m_substractStartTime) {
            const auto startTs = stream(packet.streamIndex()).startTime();
            if (startTs.isValid()) {
                if (packet.pts().isValid())
                    packet.setPts(packet.pts() - startTs);
                if (packet.dts().isValid())
                    packet.setDts(packet.pts() - startTs);
            }
        }
    }

    packet.setComplete(true);

    return packet;
}

void FormatContext::openOutput(const string &uri, OptionalErrorCode ec)
{
    return openOutput(uri, OutputFormat(), nullptr, ec);
}

void FormatContext::openOutput(const string &uri, Dictionary &options, OptionalErrorCode ec)
{
    auto ptr = options.release();
    try
    {
        openOutput(uri, OutputFormat(), &ptr, ec);
        options.assign(ptr);
    }
    catch (const Exception&)
    {
        options.assign(ptr);
        throw;
    }
}

void FormatContext::openOutput(const string &uri, Dictionary &&options, OptionalErrorCode ec)
{
    return openOutput(uri, options, ec);
}

void FormatContext::openOutput(const string &uri, OutputFormat format, AVDictionary **options, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!m_raw)
    {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (isOpened())
    {
        throws_if(ec, Errors::FormatAlreadyOpened);
        return;
    }

    if (format.isNull())
        format = outputFormat();
    else
        setFormat(format);

    if (format.isNull())
    {
        // Guess format
        format = guessOutputFormat(string(), uri);

        if (format.isNull())
        {
            fflog(AV_LOG_ERROR, "Can't guess output format");
            throws_if(ec, Errors::FormatNullOutputFormat);
            return;
        }
        setFormat(format);
    }

    // Fix stream flags
#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    for (size_t i = 0; i < streamsCount(); ++i) {
        auto st = stream(i);
        if (st.raw()->codec) {
            if (outputFormat().isFlags(AVFMT_GLOBALHEADER)) {
                st.raw()->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
        }
    }
    FF_ENABLE_DEPRECATION_WARNINGS
#endif

    resetSocketAccess();
    if (!(format.flags() & AVFMT_NOFILE))
    {
        int sts = avio_open2(&m_raw->pb, uri.c_str(), AVIO_FLAG_WRITE, &m_raw->interrupt_callback, options);
        if (sts < 0)
        {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }
    }

    m_uri = uri;
    m_isOpened = true;
}

void FormatContext::openOutput(CustomIO *io, OptionalErrorCode ec, size_t internalBufferSize)
{
    openCustomIOOutput(io, internalBufferSize, ec);
    if (!is_error(ec))
    {
        m_isOpened = true;
        m_uri.clear();
    }
}

void FormatContext::writeHeader(OptionalErrorCode ec)
{
    writeHeader(nullptr, ec);
}

void FormatContext::writeHeader(Dictionary &options, OptionalErrorCode ec)
{
    auto ptr = options.release();

    // Be exception safe
    ScopeOutAction onReturn([&ptr, &options](){
        options.assign(ptr);
    });

    writeHeader(&ptr, ec);
}

void FormatContext::writeHeader(Dictionary &&options, OptionalErrorCode ec)
{
    writeHeader(options, ec);
}


void FormatContext::writeHeader(AVDictionary **options, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!isOpened())
    {
        throws_if(ec, Errors::FormatNotOpened);
        return;
    }

    if (!isOutput())
    {
        throws_if(ec, Errors::FormatInvalidDirection);
        return;
    }

    resetSocketAccess();
    int sts = avformat_write_header(m_raw, options);
    sts = checkPbError(sts);
    if (sts < 0)
        throws_if(ec, sts, ffmpeg_category());

    m_headerWriten = true;
}

void FormatContext::writePacket(OptionalErrorCode ec)
{
    writePacket(Packet(), ec);
}

void FormatContext::writePacket(const Packet &pkt, OptionalErrorCode ec)
{
    writePacket(pkt, ec, av_interleaved_write_frame);
}

void FormatContext::writePacketDirect(OptionalErrorCode ec)
{
    writePacketDirect(Packet(), ec);
}

void FormatContext::writePacketDirect(const Packet &pkt, OptionalErrorCode ec)
{
    writePacket(pkt, ec, av_write_frame);
}

bool FormatContext::checkUncodedFrameWriting(size_t streamIndex, error_code &ec) noexcept
{
    ec.clear();

    if (!m_raw)
    {
        ec = make_avcpp_error(Errors::Unallocated);
        return false;
    }

    if (!isOpened())
    {
        ec = make_avcpp_error(Errors::FormatNotOpened);
        return false;
    }

    if (!m_headerWriten)
    {
        fflog(AV_LOG_ERROR, "Header not writen to output. Try to call writeHeader()\n");
        ec = make_avcpp_error(Errors::FormatHeaderNotWriten);
        return false;
    }

    int sts = av_write_uncoded_frame_query(m_raw, streamIndex);
    bool result = sts < 0 ? false : true;
    if (!result)
        ec = make_ffmpeg_error(sts);

    return result;
}

bool FormatContext::checkUncodedFrameWriting(size_t streamIndex) noexcept
{
    error_code ec;
    return checkUncodedFrameWriting(streamIndex, ec);
}

void FormatContext::writeUncodedFrame(VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec)
{
    writeFrame(frame.raw(), streamIndex, ec, av_interleaved_write_uncoded_frame);
}

void FormatContext::writeUncodedFrameDirect(VideoFrame &frame, size_t streamIndex, OptionalErrorCode ec)
{
    writeFrame(frame.raw(), streamIndex, ec, av_write_uncoded_frame);
}

void FormatContext::writeUncodedFrame(AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec)
{
    writeFrame(frame.raw(), streamIndex, ec, av_interleaved_write_uncoded_frame);
}

void FormatContext::writeUncodedFrameDirect(AudioSamples &frame, size_t streamIndex, OptionalErrorCode ec)
{
    writeFrame(frame.raw(), streamIndex, ec, av_write_uncoded_frame);
}

void FormatContext::writePacket(const Packet &pkt, OptionalErrorCode ec, int(*write_proc)(AVFormatContext *, AVPacket *))
{
    clear_if(ec);

    if (!isOpened())
    {
        throws_if(ec, Errors::FormatNotOpened);
        return;
    }

    if (!isOutput())
    {
        throws_if(ec, Errors::FormatInvalidDirection);
        return;
    }

    if (!m_headerWriten)
    {
        throws_if(ec, Errors::FormatHeaderNotWriten);
        return;
    }

    // Make reference to packet
    auto writePkt = pkt;

    if (!pkt.isNull()) {
        auto streamIndex = pkt.streamIndex();
        auto st = stream(streamIndex);

        if (st.isNull()) {
            fflog(AV_LOG_WARNING, "Required stream does not exists: %d, total=%ld\n", streamIndex, streamsCount());
            throws_if(ec, Errors::FormatInvalidStreamIndex);
            return;
        }

        // Set packet time base to stream one
        if (st.timeBase() != pkt.timeBase()) {
            writePkt.setTimeBase(st.timeBase());
        }
    }

    resetSocketAccess();
    int sts = write_proc(m_raw, writePkt.isNull() ? nullptr : writePkt.raw());
    sts = checkPbError(sts);
    if (sts < 0)
        throws_if(ec, sts, ffmpeg_category());
}

void FormatContext::writeFrame(AVFrame *frame, int streamIndex, OptionalErrorCode ec, int (*write_proc)(AVFormatContext *, int, AVFrame *))
{
    clear_if(ec);

    if (!isOpened())
    {
        throws_if(ec, Errors::FormatNotOpened);
        return;
    }

    if (!isOutput())
    {
        throws_if(ec, Errors::FormatInvalidDirection);
        return;
    }

    if (streamIndex < 0 || (size_t)streamIndex > streamsCount())
    {
        throws_if(ec, Errors::FormatInvalidStreamIndex);
        return;
    }

    resetSocketAccess();
    int sts = write_proc(m_raw, streamIndex, frame);
    sts = checkPbError(sts);
    if (sts < 0)
        throws_if(ec, sts, ffmpeg_category());
}

void FormatContext::writeTrailer(OptionalErrorCode ec)
{
    clear_if(ec);

    if (!isOpened())
    {
        throws_if(ec, Errors::FormatNotOpened);
        return;
    }

    if (!isOutput())
    {
        throws_if(ec, Errors::FormatInvalidDirection);
        return;
    }

    if (!m_headerWriten)
    {
        throws_if(ec, Errors::FormatHeaderNotWriten);
        return;
    }

    resetSocketAccess();
    auto sts = av_write_trailer(m_raw);
    sts = checkPbError(sts);
    if (sts < 0)
        throws_if(ec, sts, ffmpeg_category());
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

void FormatContext::findStreamInfo(AVDictionary **options, size_t optionsCount, OptionalErrorCode ec)
{
    clear_if(ec);

    if (options && optionsCount != streamsCount())
    {
        throws_if(ec, Errors::FormatWrongCountOfStreamOptions);
        return;
    }

    // Temporary disable socket timeout
    ScopedValue<int64_t> scopedTimeoutDisable(m_socketTimeout, -1, m_socketTimeout);

    int sts = avformat_find_stream_info(m_raw, options);
    m_streamsInfoFound = true;
    if (sts >= 0 && m_raw->nb_streams > 0)
    {
        av_dump_format(m_raw, 0, m_uri.c_str(), 0);
        return;
    }
    cerr << "Could not found streams in input container\n";
    throws_if(ec, sts, ffmpeg_category());
}

void FormatContext::closeCodecContexts()
{
    // HACK: This is hack to correct cleanup codec contexts in independ way
#if !USE_CODECPAR
    FF_DISABLE_DEPRECATION_WARNINGS
    auto nb = m_raw->nb_streams;
    for (size_t i = 0; i < nb; ++i) {
        auto st = m_raw->streams[i];
        auto ctx = st->codec;
        avcodec_close(ctx);
    }
    FF_ENABLE_DEPRECATION_WARNINGS
#endif
}

int FormatContext::checkPbError(int stat)
{
    // WORKAROUND: a lot of format specific writer_packet() functions always return zero code
    // and av_write_frame() in FFMPEG prio 1.0 does not contain follow wrapper
    // so, we can't detect any write error :-(
    // This workaround should fix this problem
    if (stat >= 0 && m_raw->pb && m_raw->pb->error < 0)
        return m_raw->pb->error;
    return stat;
}

void FormatContext::openCustomIO(CustomIO *io, size_t internalBufferSize, bool isWritable, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw)
    {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (isOpened())
    {
        throws_if(ec, Errors::FormatAlreadyOpened);
        return;
    }

    resetSocketAccess();

    AVIOContext *ctx = nullptr;
    // Note: buffer must be allocated only with av_malloc() and friends
    uint8_t *internalBuffer = (uint8_t*)av_mallocz(internalBufferSize);
    if (!internalBuffer)
    {
        throws_if(ec, ENOMEM, std::system_category());
        return;
    }

    ctx = avio_alloc_context(internalBuffer, internalBufferSize, isWritable, (void*)(io), custom_io_read, custom_io_write, custom_io_seek);
    if (ctx)
    {
        ctx->seekable = io->seekable();
        m_raw->flags |= AVFMT_FLAG_CUSTOM_IO;
        m_customIO = true;
    }
    else
    {
        throws_if(ec, ENOMEM, std::system_category());
        return;
    }

    m_raw->pb = ctx;
}

void FormatContext::openCustomIOInput(CustomIO *io, size_t internalBufferSize, OptionalErrorCode ec)
{
    if (isOpened())
    {
        throws_if(ec, Errors::FormatAlreadyOpened);
        return;
    }
    openCustomIO(io, internalBufferSize, false, ec);
}

void FormatContext::openCustomIOOutput(CustomIO *io, size_t internalBufferSize, OptionalErrorCode ec)
{
    if (isOpened())
    {
        throws_if(ec, Errors::FormatAlreadyOpened);
        return;
    }

    if (m_raw)
    {
        OutputFormat format = outputFormat();
        if (format.isNull())
        {
            fflog(AV_LOG_ERROR, "You must set output format for use with custom IO\n");
            throws_if(ec, Errors::FormatNullOutputFormat);
            return;
        }
    }
    openCustomIO(io, internalBufferSize, true, ec);
}

} // namespace av
