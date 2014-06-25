#include "packet.h"

namespace av {

Packet::Packet()
{
    ctorInitCommon();
}

Packet::Packet(const Packet &packet)
    : Packet()
{
    ctorInitFromAVPacket(&packet.m_pkt);
    m_completeFlag = packet.m_completeFlag;
    m_timeBase = packet.m_timeBase;
    m_fakePts = packet.m_fakePts;
}

Packet::Packet(Packet &&packet)
    : m_pkt(packet.m_pkt),
      m_completeFlag(packet.m_completeFlag),
      m_timeBase(packet.m_timeBase),
      m_fakePts(packet.m_fakePts)
{
    av_init_packet(&packet.m_pkt);
    packet.m_pkt.data = nullptr;
    packet.m_pkt.size = 0;
}

Packet::Packet(const AVPacket *packet)
    : Packet()
{
    ctorInitFromAVPacket(packet);
}

Packet::Packet(const vector<uint8_t> &data)
{
    ctorInitCommon();

    m_pkt.size = data.size();
    m_pkt.data = reinterpret_cast<uint8_t*>(av_malloc(data.size() + FF_INPUT_BUFFER_PADDING_SIZE));
    std::copy(data.begin(), data.end(), m_pkt.data);
    std::fill_n(m_pkt.data + m_pkt.size, FF_INPUT_BUFFER_PADDING_SIZE, '\0');

    m_completeFlag = true;
}

Packet::Packet(const uint8_t *data, size_t size, bool doAllign)
{
    ctorInitCommon();

    m_pkt.size = size;
    if (doAllign)
    {
        m_pkt.data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        std::fill_n(m_pkt.data + m_pkt.size, FF_INPUT_BUFFER_PADDING_SIZE, '\0');
    }
    else
        m_pkt.data = reinterpret_cast<uint8_t*>(av_malloc(size));

    std::copy(data, data + size, m_pkt.data);

    m_completeFlag = true;
}

Packet::~Packet()
{
    av_free_packet(&m_pkt);
}

void Packet::ctorInitCommon()
{
    av_init_packet(&m_pkt);
    m_pkt.data = nullptr;
    m_pkt.size = 0;

    m_completeFlag = false;
    m_timeBase     = Rational(0, 0);
    m_fakePts      = AV_NOPTS_VALUE;
}

void Packet::ctorInitFromAVPacket(const AVPacket *packet)
{
    if (!packet || packet->size <= 0)
    {
        return;
    }

    av_free_packet(&m_pkt);
    av_init_packet(&m_pkt);
    av_copy_packet(&m_pkt, const_cast<AVPacket*>(packet));

    m_fakePts      = packet->pts;
    m_completeFlag = true;
}

bool Packet::setData(const vector<uint8_t> &newData)
{
    return setData(newData.data(), newData.size());
}

bool Packet::setData(const uint8_t *newData, size_t size)
{
    if (m_pkt.size != size || m_pkt.data == 0)
    {
        if (m_pkt.buf)
            av_buffer_unref(&m_pkt.buf);
        else
            av_freep(&m_pkt.data);
        m_pkt.data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        m_pkt.size = size;

        std::fill_n(m_pkt.data + m_pkt.size, FF_INPUT_BUFFER_PADDING_SIZE, '\0'); // set padding memory to zero
    }

    std::copy(newData, newData + size, m_pkt.data);

    m_completeFlag = true;

    return true;
}

int64_t Packet::getPts() const
{
    return m_pkt.pts;
}

int64_t Packet::getDts() const
{
    return m_pkt.dts;
}

int64_t Packet::getFakePts() const
{
    return m_fakePts;
}

int32_t Packet::getSize() const
{
    return m_pkt.size;
}

void Packet::setPts(int64_t pts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        m_pkt.pts = pts;
    else
        m_pkt.pts = tsTimeBase.rescale(pts, m_timeBase);
    setFakePts(pts, tsTimeBase);
}

void Packet::setDts(int64_t dts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        m_pkt.dts = dts;
    else
        m_pkt.dts = tsTimeBase.rescale(dts, m_timeBase);
}

void Packet::setFakePts(int64_t pts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0, 0))
        m_fakePts = pts;
    else
        m_fakePts = tsTimeBase.rescale(pts, m_timeBase);
}

int Packet::getStreamIndex() const
{
    return  m_pkt.stream_index;
}

int Packet::getFlags()
{
    return m_pkt.flags;
}

bool Packet::isKeyPacket() const
{
    return (m_pkt.flags & AV_PKT_FLAG_KEY);
}

int Packet::getDuration() const
{
    return m_pkt.duration;
}

bool Packet::isComplete() const
{
    return m_completeFlag && m_pkt.data;
}

void Packet::setStreamIndex(int idx)
{
    m_pkt.stream_index = idx;
}

void Packet::setKeyPacket(bool keyPacket)
{
    if (keyPacket)
        m_pkt.flags |= AV_PKT_FLAG_KEY;
    else
        m_pkt.flags &= ~AV_PKT_FLAG_KEY;
}

void Packet::setFlags(int flags)
{
    m_pkt.flags = flags;
}

void Packet::addFlags(int flags)
{
    m_pkt.flags |= flags;
}

void Packet::clearFlags(int flags)
{
    m_pkt.flags &= ~flags;
}

void Packet::dump(const StreamPtr &st, bool dumpPayload) const
{
    if (st)
    {
        AVStream *stream = st->getAVStream();
        av_pkt_dump2(stdout, const_cast<AVPacket*>(&m_pkt), dumpPayload ? 1 : 0, stream);
    }
}

void Packet::setTimeBase(const Rational &value)
{
    if (m_timeBase == value)
        return;

    int64_t rescaledPts      = AV_NOPTS_VALUE;
    int64_t rescaledDts      = AV_NOPTS_VALUE;
    int64_t rescaledFakePts  = AV_NOPTS_VALUE;
    int     rescaledDuration = 0;

    if (m_timeBase != Rational() && value != Rational())
    {
        if (m_pkt.pts != AV_NOPTS_VALUE)
            rescaledPts = m_timeBase.rescale(m_pkt.pts, value);

        if (m_pkt.dts != AV_NOPTS_VALUE)
            rescaledDts = m_timeBase.rescale(m_pkt.dts, value);

        if (m_fakePts != AV_NOPTS_VALUE)
            rescaledFakePts = m_timeBase.rescale(m_fakePts, value);

        if (m_pkt.duration != 0)
            rescaledDuration = m_timeBase.rescale(m_pkt.duration, value);
    }
    else // Do not rescale for invalid timeBases
    {
        rescaledPts      = m_pkt.pts;
        rescaledDts      = m_pkt.dts;
        rescaledFakePts  = m_fakePts;
        rescaledDuration = m_pkt.duration;
    }

    // may be throw
    m_timeBase = value;

    // next operations non-thrown
    m_pkt.pts      = rescaledPts;
    m_pkt.dts      = rescaledDts;
    m_pkt.duration = rescaledDuration;
    m_fakePts      = rescaledFakePts;
}

void Packet::setComplete(bool complete)
{
    m_completeFlag = complete;
    // TODO: we need set packet size here - this is indicator for complete packet
}

Packet &Packet::operator =(const Packet &rhs)
{
    if (&rhs == this)
        return *this;

    Packet(rhs).swap(*this);

    return *this;
}

Packet &Packet::operator=(Packet &&rhs)
{
    if (&rhs == this)
        return *this;

    Packet(std::move(rhs)).swap(*this); // move ctor

    return *this;
}

Packet &Packet::operator =(const AVPacket &rhs)
{
    if (&rhs == &m_pkt)
        return *this;

    ctorInitFromAVPacket(&rhs);
    m_timeBase = Rational();
    return *this;
}

void Packet::swap(Packet &other)
{
    using std::swap;
    swap(m_pkt,          other.m_pkt);
    swap(m_completeFlag, other.m_completeFlag);
    swap(m_timeBase,     other.m_timeBase);
    swap(m_fakePts,      other.m_fakePts);
}

int Packet::allocatePayload(int32_t size)
{
    if (m_pkt.data && m_pkt.size != size)
    {
        return reallocatePayload(size);
    }
}

int Packet::reallocatePayload(int32_t newSize)
{
}

void Packet::setDuration(int duration, const Rational &durationTimeBase)
{
    if (durationTimeBase == Rational())
        m_pkt.duration = duration;
    else
        m_pkt.duration = durationTimeBase.rescale(duration, m_timeBase);
}

} // ::av
