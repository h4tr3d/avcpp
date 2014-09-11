#include <cassert>
#include <exception>
#include <stdexcept>
#include <functional>

#include "container.h"
#include "avutils.h"
#include "avtime.h"

using namespace std;

namespace av
{

static const StreamPtr invalidStream;

struct ContainerPriv
{
    ContainerPriv()
        : flags(0),
          state(STATE_INIT),
          containerIndex(0)
    {}


    enum {
        FLAG_READ              = 0x01,
        FLAG_WRITE             = 0x02,
        FLAG_CUSTOM_IO         = 0x04,
        FLAG_STREAMS_REQUESTED = 0x08
    };
    uint64_t flags;


    enum {
        STATE_INIT,
        STATE_OPENING,
        STATE_READY,
        STATE_WRITING,
        STATE_READING,
        STATE_CLOSING
    };
    uint64_t state;

    uint containerIndex;
};


int avioInterruptCallback(void *ptr)
{
    if (!ptr)
        return 1;

    Container *c = reinterpret_cast<Container*>(ptr);
    return c->avioInterruptHandler();
}



int writer_write(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
    {
        return -1;
    }

    AbstractWriteFunctor *writer = reinterpret_cast<AbstractWriteFunctor*>(opaque);
    return (*writer)(buf, buf_size);
}


int avio_open_writer(AVIOContext **s, const AbstractWriteFunctor &ftor, int internalBufferSize = 200000)
{
    AVIOContext *ctx = 0;
    uint8_t *internalBuffer = new uint8_t[internalBufferSize];

    ctx = avio_alloc_context(internalBuffer, internalBufferSize, 1,
                             (void*)&ftor,
                             0,
                             writer_write,
                             0);

    if (ctx)
    {
        ctx->seekable = 0;
    }

    *s = ctx;

    return ctx ? 0 : -1;
}


int avio_close_custom_io(AVIOContext *ctx)
{
    if (!ctx)
        return -1;

    delete[] ctx->buffer;
    ctx->buffer = 0;
    ctx->buffer_size = 0;
    ctx->buf_end = 0;
    ctx->buf_ptr = 0;

    av_free(ctx);
    return 0;
}


Container::Container()
    : readingTimeout(-1)
{
    init();
}

Container::Container(const ContainerFormatPtr &containerFormat)
    : readingTimeout(-1)
{
    init();
}

Container::~Container()
{
    reset();
}


bool Container::openInput(const char *uri, const ContainerFormatPtr &inputFormat)
{
    if (priv->state != ContainerPriv::STATE_INIT || context)
    {
        reset();
    }

    if (!context)
    {
        init();
    }

    priv->flags |= ContainerPriv::FLAG_READ;
    priv->flags &= ~ContainerPriv::FLAG_CUSTOM_IO;

    {
        ScopedValue<uint64_t> scopedState(priv->state, ContainerPriv::STATE_OPENING, ContainerPriv::STATE_INIT);
        lastStartReadFrameTime = std::chrono::system_clock::now();
        int stat = -1;
        if (inputFormat && inputFormat->getInputFormat())
        {
            stat = avformat_open_input(&context, uri, inputFormat->getInputFormat(), 0);
        }
        else
        {
            stat = avformat_open_input(&context, uri, 0, 0);
        }

        if (stat < 0)
        {
            return false;
        }
    }

    priv->state = ContainerPriv::STATE_READY;

    this->uri = string(uri);

    format = ContainerFormatPtr(new ContainerFormat(context->iformat, context->oformat));
    queryInputStreams();

    return true;
}

const StreamPtr &Container::getStream(uint32_t index)
{
    if (index < streams.size())
        return streams[index];

    return invalidStream;
}


int32_t Container::readNextPacket(PacketPtr &pkt)
{
    if (!priv ||
        priv->state == ContainerPriv::STATE_INIT ||
        !(priv->flags & ContainerPriv::FLAG_READ) ||
        !context)
    {
        return -1;
    }

    ScopedValue<uint64_t> scopedState(priv->state, ContainerPriv::STATE_READING, ContainerPriv::STATE_READY);

    //PacketPtr packet(new Packet());
    Packet packet;

    int stat = 0;
    int tries = 0;
    const int retryCount = 5; // TODO: вынести в настройку класса
    do
    {
        lastStartReadFrameTime = std::chrono::system_clock::now();
        stat = av_read_frame(context, packet.getAVPacket());
        ++tries;
    }
    while (stat == AVERROR(EAGAIN) && (retryCount < 0 || tries <= retryCount));

    //*(pkt.get()) = *(packet.get());
    //pkt = std::move(packet);
    //pkt = make_shared<Packet>(*packet);
    *pkt = std::move(packet);

    if (pkt->getStreamIndex() >= 0)
    {
        const StreamPtr& st = getStream(pkt->getStreamIndex());
        if (st)
        {
            pkt->setTimeBase(st->getTimeBase());
        }
    }

    return stat;
}

void Container::setReadingTimeout(int64_t value)
{
    readingTimeout = value;
}

int64_t Container::getReadingTimeout() const
{
    return readingTimeout;
}

const StreamPtr Container::addNewStream(const CodecPtr &codec)
{
    const AVCodec *avCodec = codec ? codec->getAVCodec() : 0;
    AVStream *st = 0;

    st = avformat_new_stream(context, avCodec);
    if (!st)
    {
        return invalidStream;
    }

    // HACK
    st->pts.den = 1;

    StreamPtr stream = make_shared<Stream>(shared_from_this(), st, ENCODING, nullptr);
    if (stream)
    {
        streams.push_back(stream);
        return stream;
    }

    return invalidStream;
}

bool Container::openOutput(const char *uri)
{
    priv->flags |=  ContainerPriv::FLAG_WRITE;
    priv->flags &= ~ContainerPriv::FLAG_CUSTOM_IO;

    {
        ScopedValue<uint64_t> scopedState(priv->state, ContainerPriv::STATE_OPENING, ContainerPriv::STATE_INIT);
        if (!context)
            return false;

        // TODO: если формат не определён, попытаться самим его определить по типу файла
        if (!format)
            return false;

        //AVOutputFormat *fmt = format->getOutputFormat();

        lastStartReadFrameTime = std::chrono::system_clock::now();
        int stat = avio_open2(&context->pb, uri, AVIO_FLAG_WRITE, 0, 0);
        if (stat < 0)
            return false;
    }

    priv->state  =  ContainerPriv::STATE_READY;

    this->uri = uri;

    return true;
}

bool Container::openOutput(const AbstractWriteFunctor &writer)
{
    priv->flags |= ContainerPriv::FLAG_WRITE | ContainerPriv::FLAG_CUSTOM_IO;

    {
        ScopedValue<uint64_t> scopedState(priv->state, ContainerPriv::STATE_OPENING, ContainerPriv::STATE_INIT);

        if (!context)
            return false;

        if (!format)
            return false;

        lastStartReadFrameTime = std::chrono::system_clock::now();
        int stat = avio_open_writer(&context->pb, writer);
        if (stat < 0)
            return false;
    }

    priv->state = ContainerPriv::STATE_READY;

    this->uri = writer.name();

    return true;
}

bool Container::writeHeader()
{
    if (priv->state != ContainerPriv::STATE_INIT && (priv->flags & ContainerPriv::FLAG_WRITE) && context)
    {
        return !!avformat_write_header(context, 0);
    }

    return false;
}

int Container::writePacket(const PacketPtr &packet, bool interleaveWrite)
{
    if (priv->state == ContainerPriv::STATE_INIT || !(priv->flags & ContainerPriv::FLAG_WRITE) || !context)
    {
        return -1;
    }

    int index = packet->getStreamIndex();

    // Rescale packet PTS/DTS to stream time base
    const StreamPtr& st = getStream(index);
    if (st == invalidStream)
    {
        // write to incorrect stream
        return -1;
    }

//    clog << "Index: " << st->getIndex() << "/" << index << endl;
//    clog << "Stream TB: " << st->getTimeBase() << ", Packet TB: " << packet->getTimeBase() << endl;

    if (st->getTimeBase() != packet->getTimeBase())
    {
        packet->setTimeBase(st->getTimeBase());
    }

//    clog
//            << "Packet (1): "
//            << "pts = " << packet->getPts()
//            << ", dts = " << packet->getDts()
//            << ", fake pts = " << packet->getFakePts()
//            << ", tb = " << packet->getTimeBase()
//            << ", time = " << (packet->getPts() != AV_NOPTS_VALUE ?
//                                                       packet->getPts() * packet->getTimeBase().getDouble() :
//                                                       packet->getFakePts() * packet->getTimeBase().getDouble())
//            << ", duration = " << packet->getDuration()
//            << ", duration time = " << packet->getDuration() * packet->getTimeBase().getDouble()
//            << std::endl;

    // Fix muxing issues for FLV and same formats
    if (packet->getPts() == AV_NOPTS_VALUE && packet->getFakePts() != AV_NOPTS_VALUE)
        packet->setPts(packet->getFakePts());

    int stat;
    if (interleaveWrite)
    {
        stat = av_interleaved_write_frame(context, packet->getAVPacket());
    }
    else
    {
        stat = av_write_frame(context, packet->getAVPacket());
    }

//    clog
//            << "Packet (2): "
//            << "pts = " << packet->getPts()
//            << ", dts = " << packet->getDts()
//            << ", fake pts = " << packet->getFakePts()
//            << ", tb = " << packet->getTimeBase()
//            << ", time = " << (packet->getPts() != AV_NOPTS_VALUE ?
//                                                       packet->getPts() * packet->getTimeBase().getDouble() :
//                                                       packet->getFakePts() * packet->getTimeBase().getDouble())
//            << ", duration = " << packet->getDuration()
//            << ", duration time = " << packet->getDuration() * packet->getTimeBase().getDouble()
//            << std::endl;


    // WORKAROUND: a lot of format specific writer_packet() functions always return zero code
    //             and av_write_frame() in FFMPEG prio 1.0 does not contain follow wrapper
    //             so, we can't detect any write error :-(
    // This workaround should fix this problem
    if (context->pb && context->pb->error < 0)
    {
        stat = context->pb->error;
    }

    return stat;
}

int Container::writeTrailer()
{
    if (priv->state == ContainerPriv::STATE_INIT || !(priv->flags & ContainerPriv::FLAG_WRITE) || !context)
    {
        return -1;
    }

    int stat = av_write_trailer(context);

    // WORKAROUND: a lot of format specific writer_packet() functions always return zero code
    //             so, we can't detect any write error :-(
    // This workaround should fix this problem
    if (context->pb && context->pb->error < 0)
    {
        stat = context->pb->error;
    }

    return stat;
}

void Container::flush()
{
    if (priv->state != ContainerPriv::STATE_INIT && context && context->pb)
    {
        avio_flush(context->pb);
    }
}

void Container::close()
{
    if (priv->state != ContainerPriv::STATE_INIT && context)
    {
        {
            ScopedValue<uint64_t> scopedState(priv->state, ContainerPriv::STATE_CLOSING, priv->state);
            if (context->pb)
            {
                if (priv->flags & ContainerPriv::FLAG_CUSTOM_IO)
                    avio_close_custom_io(context->pb);
                else
                    avio_close(context->pb);

                context->pb = 0;
            }
            else if (priv->flags & ContainerPriv::FLAG_READ)
            {
                avformat_close_input(&context);
                context = 0;
            }
        }

        priv->state = ContainerPriv::STATE_INIT;
    }
}

bool Container::isOpened() const
{
    return (priv->state != ContainerPriv::STATE_INIT);
}

void Container::setFormat(const ContainerFormatPtr &newFormat)
{
    if (priv->state != ContainerPriv::STATE_INIT)
    {
        cout << "Can't set format for opened container\n";
        return;
    }

    if (!context)
        init();

    if (context)
    {
        format = newFormat;

        if (format->getInputFormat())
        {
            context->iformat = format->getInputFormat();
            context->oformat = 0;
        }
        else
        {
            /*reset();
            context = 0;
            if (avformat_alloc_output_context2(&context, format->getOutputFormat(), 0, 0) < 0)
            {
                // TODO: пересмотреть диагностику
                throw std::runtime_error("can't allocate output context");
            }*/
            context->iformat = 0;
            context->oformat = format->getOutputFormat();
        }
    }
}

AVFormatContext *Container::getAVFromatContext()
{
    return context;
}

void Container::setIndex(uint index)
{
    priv->containerIndex = index;
}

uint Container::getIndex() const
{
    return priv->containerIndex;
}

void Container::dump()
{
    if (context)
    {
        av_dump_format(context, priv->containerIndex, uri.c_str(), !!(context->oformat));
    }
}

void Container::setupInterruptHandling()
{
    // Set up thread interrupt handling
    context->interrupt_callback.callback = avioInterruptCallback;
    context->interrupt_callback.opaque   = this;
}

int Container::avioInterruptHandler()
{
    // check timeout for reading only
    if (priv->flags & ContainerPriv::FLAG_READ &&
        (priv->state == ContainerPriv::STATE_OPENING || priv->state == ContainerPriv::STATE_READING))
    {
        std::chrono::time_point<std::chrono::system_clock> currentTime = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsedTime = currentTime - lastStartReadFrameTime;

        if (elapsedTime.count() > readingTimeout && readingTimeout > -1)
        {
            std::cerr << "Reading timeout" << std::endl;
            return 1;
        }
    }

    return 0;
}

void Container::queryInputStreams()
{
    if (priv->state != ContainerPriv::STATE_READY)
    {
        cout << "Container does not opened\n";
        return;
    }

    assert(context && "Uninitialized context");

    int stat = 0;

    if (!(priv->flags & ContainerPriv::FLAG_STREAMS_REQUESTED))
    {
        stat = avformat_find_stream_info(context, 0);
        priv->flags |= ContainerPriv::FLAG_STREAMS_REQUESTED;
    }

    if (stat >= 0 && context->nb_streams > 0)
    {
        setupInputStreams();
    }
    else
    {
        cout << "Could not found streams in input container\n";
    }
}

void Container::setupInputStreams()
{
    if (streams.size() == context->nb_streams)
        return;

    Rational timeBase;
    for (uint32_t i = 0; i < context->nb_streams; ++i)
    {
        AVStream *st = context->streams[i];
        if (st && st->time_base.den && st->time_base.num)
        {
            timeBase = st->time_base;
            break;
        }
    }

    for (uint32_t i = 0; i < context->nb_streams; ++i)
    {
        AVStream *st = context->streams[i];
        if (!st)
            continue;

        if (timeBase != Rational() && (!st->time_base.num || !st->time_base.den))
        {
            st->time_base = timeBase.getValue();
        }

        StreamPtr stream = make_shared<Stream>(shared_from_this(), st, DECODING, nullptr);
        if (stream)
        {
            streams.push_back(stream);
        }
    }

    av_dump_format(context, priv->containerIndex, uri.c_str(), 0);
}


void Container::init()
{
    priv    = new ContainerPriv;
    context = avformat_alloc_context();

    setupInterruptHandling();
}

void Container::reset()
{
    if (priv->state != ContainerPriv::STATE_INIT)
    {
        close();
    }

    if (context)
    {
        avformat_free_context(context);
    }

    if (priv)
    {
        delete priv;
    }

    context = 0;
}



} // ::av
