#include "packet.h"
#include "avutils.h"

using namespace std;

namespace av {

Packet::Packet()
{
    initCommon();
}

Packet::Packet(const Packet &packet, OptionalErrorCode ec)
    : Packet()
{
    initFromAVPacket(&packet.m_raw, false, ec);
    m_completeFlag = packet.m_completeFlag;
    m_timeBase = packet.m_timeBase;
}

Packet::Packet(Packet &&packet)
    : m_completeFlag(packet.m_completeFlag),
      m_timeBase(packet.m_timeBase)
{
    // copy packet as is
    m_raw = packet.m_raw;
    av_init_packet(&packet.m_raw);
}

Packet::Packet(const AVPacket *packet, OptionalErrorCode ec)
    : Packet()
{
    initFromAVPacket(packet, false, ec);
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
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE));
        std::fill_n(m_raw.data + m_raw.size, AV_INPUT_BUFFER_PADDING_SIZE, '\0');
    }
    else
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size));

    std::copy(data, data + size, m_raw.data);

    m_raw.buf = av_buffer_create(m_raw.data, m_raw.size, av_buffer_default_free, nullptr, 0);

    m_completeFlag = true;
}

Packet::~Packet()
{
    avpacket_unref(&m_raw);
}

void Packet::initCommon()
{
    av_init_packet(&m_raw);

    m_raw.stream_index = -1; // no stream

    m_completeFlag = false;
    m_timeBase     = Rational(0, 0);
}

FF_DISABLE_DEPRECATION_WARNINGS
void Packet::initFromAVPacket(const AVPacket *packet, bool deepCopy, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!packet || packet->size <= 0)
    {
        return;
    }

    avpacket_unref(&m_raw);
    av_init_packet(&m_raw);

    AVPacket tmp = *packet;
    if (deepCopy) {
        // Preven referencing instead of deep copy
        tmp.buf = nullptr;
    }

    int sts = av_copy_packet(&m_raw, &tmp);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }

    m_completeFlag = m_raw.size > 0;
}
FF_ENABLE_DEPRECATION_WARNINGS

bool Packet::setData(const vector<uint8_t> &newData, OptionalErrorCode ec)
{
    return setData(newData.data(), newData.size(), ec);
}

bool Packet::setData(const uint8_t *newData, size_t size, OptionalErrorCode ec)
{
    clear_if(ec);
    if ((m_raw.size >= 0 && (size_t)m_raw.size != size) || m_raw.data == 0)
    {
        if (m_raw.buf)
            av_buffer_unref(&m_raw.buf);
        else
            av_freep(&m_raw.data);
        m_raw.data = reinterpret_cast<uint8_t*>(av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE));
        m_raw.size = size;

        std::fill_n(m_raw.data + m_raw.size, AV_INPUT_BUFFER_PADDING_SIZE, '\0'); // set padding memory to zero
        m_raw.buf = av_buffer_create(m_raw.data, m_raw.size, av_buffer_default_free, nullptr, 0);
        if (!m_raw.buf) {
            throws_if(ec, ENOMEM, system_category());
            return false;
        }
    }

    std::copy(newData, newData + size, m_raw.data);

    m_completeFlag = true;

    return true;
}

Timestamp Packet::pts() const
{
    return {m_raw.pts, m_timeBase};
}

Timestamp Packet::dts() const
{
    return {m_raw.dts, m_timeBase};
}

Timestamp Packet::ts() const
{
    return {m_raw.pts != av::NoPts ? m_raw.pts : m_raw.dts, m_timeBase};
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
}

void Packet::setDts(int64_t dts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        m_raw.dts = dts;
    else
        m_raw.dts = tsTimeBase.rescale(dts, m_timeBase);
}

void Packet::setPts(const Timestamp &pts)
{
    m_raw.pts = pts.timestamp(m_timeBase);
}

void Packet::setDts(const Timestamp &dts)
{
    m_raw.dts = dts.timestamp(m_timeBase);
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

bool Packet::isNull() const
{
    return m_raw.data == nullptr || m_raw.size == 0;
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

void Packet::dump(const Stream &st, bool dumpPayload) const
{
    if (!st.isNull())
    {
        const AVStream *stream = st.raw();
        av_pkt_dump2(stdout, const_cast<AVPacket*>(&m_raw), dumpPayload ? 1 : 0, stream);
    }
}

void Packet::setTimeBase(const Rational &tb)
{
    if (m_timeBase == tb)
        return;

    if (m_timeBase != Rational())
        av_packet_rescale_ts(&m_raw,
                             m_timeBase.getValue(),
                             tb.getValue());

    m_timeBase = tb;
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

FF_DISABLE_DEPRECATION_WARNINGS
AVPacket Packet::makeRef(OptionalErrorCode ec) const
{
    clear_if(ec);
    AVPacket pkt;
    auto sts = av_copy_packet(&pkt, &m_raw);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
    }
    return pkt;
}
FF_ENABLE_DEPRECATION_WARNINGS

Packet Packet::clone(OptionalErrorCode ec) const
{
    Packet pkt;
    pkt.initFromAVPacket(&m_raw, true, ec);
    return pkt;
}

void Packet::setComplete(bool complete)
{
    m_completeFlag = complete;
}

Packet &Packet::operator=(const Packet &rhs)
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

Packet &Packet::operator=(const AVPacket &rhs)
{
    if (&rhs == &m_raw)
        return *this;

    initFromAVPacket(&rhs, false, throws());
    m_timeBase = Rational();
    return *this;
}

void Packet::swap(Packet &other)
{
    using std::swap;
    swap(m_raw,          other.m_raw);
    swap(m_completeFlag, other.m_completeFlag);
    swap(m_timeBase,     other.m_timeBase);
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
