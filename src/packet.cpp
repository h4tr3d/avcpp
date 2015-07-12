#include "packet.h"

using namespace std;

namespace av {

Packet::Packet()
{
    initCommon();
}

Packet::Packet(const Packet &packet)
    : Packet()
{
    initFromAVPacket(&packet.m_raw, false);
    m_completeFlag = packet.m_completeFlag;
    m_timeBase = packet.m_timeBase;
    m_fakePts = packet.m_fakePts;
}

Packet::Packet(Packet &&packet)
    : m_completeFlag(packet.m_completeFlag),
      m_timeBase(packet.m_timeBase),
      m_fakePts(packet.m_fakePts)
{
    // copy packet as is
    m_raw = packet.m_raw;
    av_init_packet(&packet.m_raw);
}

Packet::Packet(const AVPacket *packet)
    : Packet()
{
    initFromAVPacket(packet, false);
}

Packet::Packet(const vector<uint8_t> &data)
    : Packet(data.data(), data.size(), true)
{
}

Packet::Packet(const uint8_t *data, size_t size, bool doAllign)
    : Packet()
{
    m_raw.size = size;
    if (doAllign)
    {
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        std::fill_n(m_raw.data + m_raw.size, FF_INPUT_BUFFER_PADDING_SIZE, '\0');
    }
    else
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size));

    std::copy(data, data + size, m_raw.data);

    m_raw.buf = av_buffer_create(m_raw.data, m_raw.size, av_buffer_default_free, nullptr, 0);

    m_completeFlag = true;
}

Packet::~Packet()
{
    av_free_packet(&m_raw);
}

void Packet::initCommon()
{
    av_init_packet(&m_raw);

    m_completeFlag = false;
    m_timeBase     = Rational(0, 0);
    m_fakePts      = AV_NOPTS_VALUE;
}

void Packet::initFromAVPacket(const AVPacket *packet, bool deepCopy)
{
    if (!packet || packet->size <= 0)
    {
        return;
    }

    av_free_packet(&m_raw);
    av_init_packet(&m_raw);

    AVPacket tmp = *packet;
    if (deepCopy) {
        // Preven referencing instead of deep copy
        tmp.buf = nullptr;
    }

    av_copy_packet(&m_raw, &tmp);

    m_fakePts      = packet->pts;
    m_completeFlag = m_raw.size > 0;
}

bool Packet::setData(const vector<uint8_t> &newData)
{
    return setData(newData.data(), newData.size());
}

bool Packet::setData(const uint8_t *newData, size_t size)
{
    if ((m_raw.size >= 0 && (size_t)m_raw.size != size) || m_raw.data == 0)
    {
        if (m_raw.buf)
            av_buffer_unref(&m_raw.buf);
        else
            av_freep(&m_raw.data);
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE));
        m_raw.size = size;

        std::fill_n(m_raw.data + m_raw.size, FF_INPUT_BUFFER_PADDING_SIZE, '\0'); // set padding memory to zero
        m_raw.buf = av_buffer_create(m_raw.data, m_raw.size, av_buffer_default_free, nullptr, 0);
    }

    std::copy(newData, newData + size, m_raw.data);

    m_completeFlag = true;

    return true;
}

int64_t Packet::pts() const
{
    return m_raw.pts;
}

int64_t Packet::dts() const
{
    return m_raw.dts;
}

int64_t Packet::fakePts() const
{
    return m_fakePts;
}

size_t Packet::size() const
{
    return m_raw.size < 0 ? 0 : (size_t)m_raw.size;
}

void Packet::setPts(int64_t pts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        m_raw.pts = pts;
    else
        m_raw.pts = tsTimeBase.rescale(pts, m_timeBase);
    setFakePts(pts, tsTimeBase);
}

void Packet::setDts(int64_t dts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        m_raw.dts = dts;
    else
        m_raw.dts = tsTimeBase.rescale(dts, m_timeBase);
}

void Packet::setFakePts(int64_t pts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0, 0))
        m_fakePts = pts;
    else
        m_fakePts = tsTimeBase.rescale(pts, m_timeBase);
}

int Packet::streamIndex() const
{
    return  m_raw.stream_index;
}

int Packet::flags()
{
    return m_raw.flags;
}

bool Packet::isKeyPacket() const
{
    return (m_raw.flags & AV_PKT_FLAG_KEY);
}

int Packet::duration() const
{
    return m_raw.duration;
}

bool Packet::isComplete() const
{
    return m_completeFlag && m_raw.data && m_raw.size;
}

void Packet::setStreamIndex(int idx)
{
    m_raw.stream_index = idx;
}

void Packet::setKeyPacket(bool keyPacket)
{
    if (keyPacket)
        m_raw.flags |= AV_PKT_FLAG_KEY;
    else
        m_raw.flags &= ~AV_PKT_FLAG_KEY;
}

void Packet::setFlags(int flags)
{
    m_raw.flags = flags;
}

void Packet::addFlags(int flags)
{
    m_raw.flags |= flags;
}

void Packet::clearFlags(int flags)
{
    m_raw.flags &= ~flags;
}

void Packet::dump(const Stream2 &st, bool dumpPayload) const
{
    if (!st.isNull())
    {
        const AVStream *stream = st.raw();
        av_pkt_dump2(stdout, const_cast<AVPacket*>(&m_raw), dumpPayload ? 1 : 0, stream);
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
        if (m_raw.pts != AV_NOPTS_VALUE)
            rescaledPts = m_timeBase.rescale(m_raw.pts, value);

        if (m_raw.dts != AV_NOPTS_VALUE)
            rescaledDts = m_timeBase.rescale(m_raw.dts, value);

        if (m_fakePts != AV_NOPTS_VALUE)
            rescaledFakePts = m_timeBase.rescale(m_fakePts, value);

        if (m_raw.duration != 0)
            rescaledDuration = m_timeBase.rescale(m_raw.duration, value);
    }
    else // Do not rescale for invalid timeBases
    {
        rescaledPts      = m_raw.pts;
        rescaledDts      = m_raw.dts;
        rescaledFakePts  = m_fakePts;
        rescaledDuration = m_raw.duration;
    }

    // may be throw
    m_timeBase = value;

    // next operations non-thrown
    m_raw.pts      = rescaledPts;
    m_raw.dts      = rescaledDts;
    m_raw.duration = rescaledDuration;
    m_fakePts      = rescaledFakePts;
}

bool Packet::isReferenced() const
{
    return m_raw.buf;
}

int Packet::refCount() const
{
    if (m_raw.buf)
        return av_buffer_get_ref_count(m_raw.buf);
    else
        return 0;
}

AVPacket Packet::makeRef() const
{
    AVPacket pkt;
    av_copy_packet(&pkt, &m_raw);
    return pkt;
}

Packet Packet::clone() const
{
    Packet pkt;
    pkt.initFromAVPacket(&m_raw, true);
    return pkt;
}

void Packet::setComplete(bool complete)
{
    m_completeFlag = complete;
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
    if (&rhs == &m_raw)
        return *this;

    initFromAVPacket(&rhs, false);
    m_timeBase = Rational();
    return *this;
}

void Packet::swap(Packet &other)
{
    using std::swap;
    swap(m_raw,          other.m_raw);
    swap(m_completeFlag, other.m_completeFlag);
    swap(m_timeBase,     other.m_timeBase);
    swap(m_fakePts,      other.m_fakePts);
}

#if 0
int Packet::allocatePayload(int32_t size)
{
    if (m_raw.data && m_raw.size != size)
    {
        return reallocatePayload(size);
    }
}

int Packet::reallocatePayload(int32_t newSize)
{
}
#endif

void Packet::setDuration(int duration, const Rational &durationTimeBase)
{
    if (durationTimeBase == Rational())
        m_raw.duration = duration;
    else
        m_raw.duration = durationTimeBase.rescale(duration, m_timeBase);
}

} // ::av
