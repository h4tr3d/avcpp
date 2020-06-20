#include "packet.h"
#include "avutils.h"

using namespace std;

namespace av {

Packet::Packet()
{
    initCommon();
}

Packet::Packet(const Packet &packet)
    : Packet(packet, throws())
{
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
    av_packet_move_ref(&m_raw, &packet.m_raw);
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

Packet::Packet(const uint8_t *data, size_t size, bool /*doAllign*/)
    : Packet()
{
    auto pkt_data = av::memdup<uint8_t>(data, size);
    if (!pkt_data) {
        return;
    }

    auto sts = av_packet_from_data(&m_raw, pkt_data.get(), size);
    if (sts < 0)
        return;

    pkt_data.release();

    m_completeFlag = true;
}

Packet::Packet(uint8_t *data, size_t size, Packet::wrap_data, OptionalErrorCode ec)
    : Packet()
{
    auto sts = av_packet_from_data(&m_raw, data, size);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
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

void Packet::initFromAVPacket(const AVPacket *packet, bool deepCopy, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!packet || packet->size <= 0)
    {
        return;
    }

    avpacket_unref(&m_raw);

    if (deepCopy) {
        m_raw = *packet;
        m_raw.buf = nullptr;
        m_raw.size = 0;
        m_raw.data = nullptr;

        auto data = av::memdup<uint8_t>(packet->data, packet->size);
        if (!data) {
            throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
            return;
        }

        auto sts = av_packet_from_data(&m_raw, data.get(), packet->size);
        if (sts < 0) {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }

        // all ok, packet now own data, drop owning
        data.release();
    } else {
        auto sts = av_packet_ref(&m_raw, packet);
        if (sts < 0) {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }
    }

    m_completeFlag = m_raw.size > 0;
}

bool Packet::setData(const vector<uint8_t> &newData, OptionalErrorCode ec)
{
    return setData(newData.data(), newData.size(), ec);
}

bool Packet::setData(const uint8_t *newData, size_t size, OptionalErrorCode ec)
{
    clear_if(ec);
    av_packet_unref(&m_raw);

    auto data = av::memdup<uint8_t>(newData, size);
    if (!data) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return false;
    }

    auto sts = av_packet_from_data(&m_raw, data.get(), size);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return false;
    }

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

AVPacket Packet::makeRef(OptionalErrorCode ec) const
{
    clear_if(ec);
    AVPacket pkt;
    auto sts = av_packet_ref(&pkt, &m_raw);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
    }
    return pkt;
}

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

void Packet::setDuration(int duration, const Rational &durationTimeBase)
{
    if (durationTimeBase == Rational())
        m_raw.duration = duration;
    else
        m_raw.duration = durationTimeBase.rescale(duration, m_timeBase);
}

} // ::av
