

#include "packet.h"

namespace av {


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // <= 1.0
#define DUP_DATA(dst, src, size, padding)                               \
    do {                                                                \
        void *data;                                                     \
        if (padding) {                                                  \
            if ((unsigned)(size) >                                      \
                (unsigned)(size) + FF_INPUT_BUFFER_PADDING_SIZE)        \
                goto failed_alloc;                                      \
            data = av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);      \
        } else {                                                        \
            data = av_malloc(size);                                     \
        }                                                               \
        if (!data)                                                      \
            goto failed_alloc;                                          \
        memcpy(data, src, size);                                        \
        if (padding)                                                    \
            memset((uint8_t *)data + size, 0,                           \
                   FF_INPUT_BUFFER_PADDING_SIZE);                       \
        reinterpret_cast<uint8_t*&>(dst) =                              \
                   reinterpret_cast<uint8_t*>(data);                    \
    } while (0)


/* Makes duplicates of data, side_data, but does not copy any other fields */
static
int copy_packet_data_int(AVPacket *dst, AVPacket *src)
{
    dst->data      = NULL;
    dst->side_data = NULL;
    DUP_DATA(dst->data, src->data, dst->size, 1);
    dst->destruct = av_destruct_packet;

    if (dst->side_data_elems) {
        int i;

        DUP_DATA(dst->side_data, src->side_data,
                dst->side_data_elems * sizeof(*dst->side_data), 0);
        memset(dst->side_data, 0,
                dst->side_data_elems * sizeof(*dst->side_data));
        for (i = 0; i < dst->side_data_elems; i++) {
            DUP_DATA(dst->side_data[i].data, src->side_data[i].data,
                    src->side_data[i].size, 1);
            dst->side_data[i].size = src->side_data[i].size;
            dst->side_data[i].type = src->side_data[i].type;
        }
    }
    return 0;

failed_alloc:
    av_destruct_packet(dst);
    return AVERROR(ENOMEM);
}

static
int av_copy_packet_int(AVPacket *dst, AVPacket *src)
{
    *dst = *src;
    return copy_packet_data_int(dst, src);
}
#else
#define av_copy_packet_int av_copy_packet
#endif


void Packet::ctorInitCommon()
{
    pkt = (AVPacket*)av_malloc(sizeof(AVPacket));
    if (!pkt)
    {
        throw std::bad_alloc();
    }

    av_init_packet(pkt);
    pkt->data     = 0;
    pkt->size     = 0;
    pkt->priv     = this;
    pkt->destruct = av_destruct_packet;

    isCompleteFlag = false;
    fakePts        = AV_NOPTS_VALUE;
}

void Packet::ctorInitFromAVPacket(const AVPacket *packet)
{
    if (!packet || packet->size <= 0)
    {
        return;
    }

    const AVPacket *srcPkt     = packet;
    uint8_t        *srcBuf     = srcPkt->data;
    size_t          srcBufSize = srcPkt->size;

    // Save some variavles from original packet
    void (*origDestruct)(struct AVPacket *) = pkt->destruct;

    // free memory, omit memory leak
    if (pkt->data)
    {
        av_freep(&pkt->data);
        pkt->size = 0;
    }

    av_copy_packet_int(pkt, const_cast<AVPacket*>(srcPkt));

    // Restore/set out variables
    pkt->destruct = origDestruct;
    pkt->priv     = this;

    fakePts       = packet->pts;
    isCompleteFlag = true;
}

Packet::Packet()
{
    ctorInitCommon();
}

Packet::Packet(const Packet &packet)
{
    ctorInitCommon();
    ctorInitFromAVPacket(packet.pkt);
    timeBase = packet.timeBase;
    isCompleteFlag = packet.isCompleteFlag;
    fakePts = packet.fakePts;
}

Packet::Packet(const AVPacket *packet)
{
    ctorInitCommon();
    ctorInitFromAVPacket(packet);
}

Packet::Packet(const vector<uint8_t> &data)
{
    ctorInitCommon();

    pkt->size = data.size();
    pkt->data = reinterpret_cast<uint8_t*>(av_malloc(data.size() + FF_INPUT_BUFFER_PADDING_SIZE));
    //std::copy(data.data(), data.data() + data.size(), pkt->data);
    std::copy(data.begin(), data.end(), pkt->data);
    std::fill_n(pkt->data + pkt->size, FF_INPUT_BUFFER_PADDING_SIZE, '\0');

    isCompleteFlag = true;
}

Packet::Packet(const uint8_t *data, size_t size, bool doAllign)
{
    ctorInitCommon();

    pkt->size = size;
    if (doAllign)
    {
        pkt->data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        std::fill_n(pkt->data + pkt->size, FF_INPUT_BUFFER_PADDING_SIZE, '\0');
    }
    else
        pkt->data = reinterpret_cast<uint8_t*>(av_malloc(size));
    //std::copy(data.data(), data.data() + data.size(), pkt->data);
    std::copy(data, data + size, pkt->data);

    isCompleteFlag = true;
}

Packet::~Packet()
{
    if (pkt)
    {
        av_free_packet(pkt);
        av_free(pkt);
    }

    pkt = 0;
}

bool Packet::setData(const vector<uint8_t> &newData)
{
    return setData(newData.data(), newData.size());
}

bool Packet::setData(const uint8_t *newData, size_t size)
{
    if (!pkt)
        return false;

    if (pkt->size != size || pkt->data == 0)
    {
        av_freep(&pkt->data);
        pkt->data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        pkt->size = size;

        std::fill_n(pkt->data + pkt->size, FF_INPUT_BUFFER_PADDING_SIZE, '\0'); // set padding memory to zero
    }

    std::copy(newData, newData + size, pkt->data);

    isCompleteFlag = true;

    return true;
}

int64_t Packet::getPts() const
{
    return (pkt ? pkt->pts : AV_NOPTS_VALUE);
}

int64_t Packet::getDts() const
{
    return (pkt ? pkt->dts : AV_NOPTS_VALUE);
}

int64_t Packet::getFakePts() const
{
    return fakePts;
}

int32_t Packet::getSize() const
{
    return pkt ? pkt->size : 0;
}

void Packet::setPts(int64_t pts)
{
    if (pkt)
    {
        pkt->pts = pts;
        setFakePts(pts);
    }
}

void Packet::setDts(int64_t dts)
{
    if (pkt)
        pkt->dts = dts;
}

void Packet::setFakePts(int64_t pts)
{
    fakePts = pts;
}

int Packet::getStreamIndex() const
{
    return (pkt ? pkt->stream_index : (int32_t)-1);
}

int32_t Packet::getFlags()
{
    return (pkt ? pkt->flags : 0);
}

bool Packet::isKeyPacket() const
{
    return (pkt ? pkt->flags & AV_PKT_FLAG_KEY : false);
}

int Packet::getDuration() const
{
    return (pkt ? pkt->duration : (int64_t)-1);
}

bool Packet::isComplete() const
{
    return isCompleteFlag && pkt && pkt->data;
}

void Packet::setStreamIndex(int idx)
{
    if (pkt)
    {
        pkt->stream_index = idx;
    }
}

void Packet::setKeyPacket(bool keyPacket)
{
    if (pkt)
    {
        if (keyPacket)
            pkt->flags |= AV_PKT_FLAG_KEY;
        else
            pkt->flags = 0; // TODO: turn off only AV_PKT_FLAG_KEY!
    }
}

void Packet::setFlags(int32_t flags)
{
    if (pkt)
        pkt->flags = flags;
}

void Packet::addFlags(int32_t flags)
{
    if (pkt)
        pkt->flags |= flags;
}

void Packet::clearFlags(int32_t flags)
{
    if (pkt)
        pkt->flags &= ~flags;
}

void Packet::dump(const StreamPtr &st, bool dumpPayload) const
{
    if (pkt && st)
    {
        AVStream *stream = st->getAVStream();
        av_pkt_dump2(stdout, pkt, dumpPayload ? 1 : 0, stream);
    }
}

void Packet::setTimeBase(const Rational &value)
{
    if (timeBase == value)
        return;

    int64_t rescaledPts      = AV_NOPTS_VALUE;
    int64_t rescaledDts      = AV_NOPTS_VALUE;
    int64_t rescaledFakePts  = AV_NOPTS_VALUE;
    int     rescaledDuration = 0;

    if (pkt)
    {
        if (timeBase != Rational() && value != Rational())
        {
            if (pkt->pts != AV_NOPTS_VALUE)
                rescaledPts = timeBase.rescale(pkt->pts, value);

            if (pkt->dts != AV_NOPTS_VALUE)
                rescaledDts = timeBase.rescale(pkt->dts, value);

            if (fakePts != AV_NOPTS_VALUE)
                rescaledFakePts = timeBase.rescale(fakePts, value);

            if (pkt->duration != 0)
                rescaledDuration = timeBase.rescale(pkt->duration, value);
        }
        else // Do not rescale for invalid timeBases
        {
            rescaledPts      = pkt->pts;
            rescaledDts      = pkt->dts;
            rescaledFakePts  = fakePts;
            rescaledDuration = pkt->duration;
        }

    }

    // may be throw
    timeBase = value;

    // next operations non-thrown
    if (pkt)
    {
        pkt->pts      = rescaledPts;
        pkt->dts      = rescaledDts;
        pkt->duration = rescaledDuration;
        fakePts       = rescaledFakePts;
    }
}

void Packet::setComplete(bool complete)
{
    isCompleteFlag = complete;
    if (pkt)
    {
        // TODO: we need set packet size here - this is indicator for complete packet
    }
}

Packet &Packet::operator =(const Packet &newPkt)
{
    ctorInitFromAVPacket(newPkt.getAVPacket());
    timeBase = newPkt.getTimeBase();
    fakePts = newPkt.fakePts;
    return *this;
}

Packet &Packet::operator =(const AVPacket &newPkt)
{
    ctorInitFromAVPacket(&newPkt);
    timeBase = Rational();
    return *this;
}

int Packet::allocatePayload(int32_t size)
{
    if (!pkt)
        return -1;

    if (pkt->data && pkt->size != size)
    {
        return reallocatePayload(size);
    }


}

int Packet::reallocatePayload(int32_t newSize)
{
}

void Packet::setDuration(int duration)
{
    if (pkt)
        pkt->duration = duration;
}




} // ::av
