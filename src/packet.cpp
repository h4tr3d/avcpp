#include "packet.h"

using namespace std;

namespace av {

Packet::Packet()
{
#if API_AVCODEC_NEW_INIT_PACKET
    m_raw = av_packet_alloc();
#else
    av_init_packet(raw());
#endif

    raw()->stream_index = -1; // no stream

    m_completeFlag = false;
    m_timeBase     = Rational(0, 0);

}

Packet::Packet(const Packet &packet)
    : Packet(packet, throws())
{
}

Packet::Packet(const Packet &packet, OptionalErrorCode ec)
    : Packet()
{
    initFromAVPacket(packet.raw(), false, ec);
    m_completeFlag = packet.m_completeFlag;
    m_timeBase = packet.m_timeBase;
}

Packet::Packet(Packet &&packet)
    : Packet()
{
    m_completeFlag = packet.m_completeFlag;
    m_timeBase = packet.m_timeBase;
    av_packet_move_ref(raw(), packet.raw());
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

    auto sts = av_packet_from_data(raw(), pkt_data.get(), size);
    if (sts < 0)
        return;

    pkt_data.release();

    m_completeFlag = true;
}

Packet::Packet(uint8_t *data, size_t size, Packet::wrap_data, OptionalErrorCode ec)
    : Packet()
{
    auto sts = av_packet_from_data(raw(), data, size);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
    m_completeFlag = true;
}

static void dummy_buffer_free(void */*opaque*/, uint8_t */*ptr*/)
{}

Packet::Packet(uint8_t *data, size_t size, Packet::wrap_data_static, OptionalErrorCode ec)
    : Packet()
{
    auto buf = av_buffer_create(data, size, dummy_buffer_free, nullptr, 0);
    if (!buf) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return;
    }

    raw()->buf = buf;
    raw()->data = data;
    raw()->size = size;
    m_completeFlag = true;
}

#if AVCPP_CXX_STANDARD >= 20
Packet::Packet(std::span<const uint8_t> data, bool doAllign)
    : Packet(data.data(), data.size(), doAllign)
{}

Packet::Packet(std::span<uint8_t> data, wrap_data, OptionalErrorCode ec)
    : Packet(data.data(), data.size(), wrap_data{}, ec)
{}

Packet::Packet(std::span<uint8_t> data, wrap_data_static, OptionalErrorCode ec)
    : Packet(data.data(), data.size(), wrap_data_static{}, ec)
{}
#endif

Packet::~Packet()
{
#if API_AVCODEC_NEW_INIT_PACKET
    av_packet_free(&m_raw);
#else
    avpacket_unref(&m_raw);
#endif
}


void Packet::initFromAVPacket(const AVPacket *src, bool deepCopy, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!src || src->size <= 0)
    {
        return;
    }

    if (raw())
        avpacket_unref(raw());

    if (deepCopy) {
#if API_AVCODEC_NEW_INIT_PACKET
        if (!m_raw)
            m_raw = av_packet_alloc();

        // Copy properties first
        av_packet_copy_props(m_raw, src);

        // Referecen to the same data as src
        m_raw->data = src->data;
        m_raw->size = src->size;

        // Make new package reference counted: this allocates pkt->buf, copy data from the pkt->data
        // to the pkt->buf->data and setup pkt->data to the pkt->buf->data
        av_packet_make_refcounted(m_raw);
#else
        m_raw = *src;
        raw()->buf = nullptr;
        raw()->size = 0;
        raw()->data = nullptr;

        auto data = av::memdup<uint8_t>(src->data, src->size);
        if (!data) {
            throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
            return;
        }

        auto sts = av_packet_from_data(raw(), data.get(), src->size);
        if (sts < 0) {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }

        // all ok, packet now own data, drop owning
        data.release();
#endif
    } else {
        auto sts = av_packet_ref(raw(), src);
        if (sts < 0) {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }
    }

    m_completeFlag = raw()->size > 0;
}

bool Packet::setData(const vector<uint8_t> &newData, OptionalErrorCode ec)
{
    return setData(newData.data(), newData.size(), ec);
}

bool Packet::setData(const uint8_t *newData, size_t size, OptionalErrorCode ec)
{
    clear_if(ec);
    av_packet_unref(raw());

    auto data = av::memdup<uint8_t>(newData, size);
    if (!data) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return false;
    }

    auto sts = av_packet_from_data(raw(), data.get(), size);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return false;
    }

    data.release();

    m_completeFlag = true;

    return true;
}

const uint8_t *Packet::data() const
{
    return raw()->data;
}

uint8_t *Packet::data()
{
    return raw()->data;
}

#if AVCPP_CXX_STANDARD >= 20
bool Packet::setData(std::span<const uint8_t> newData, OptionalErrorCode ec)
{
    return setData(newData.data(), newData.size(), ec);
}

std::span<uint8_t> Packet::span()
{
    return {raw()->data, size_t(raw()->size)};
}

std::span<const uint8_t> Packet::span() const
{
    return {raw()->data, size_t(raw()->size)};
}
#endif

Timestamp Packet::pts() const
{
    return {raw()->pts, m_timeBase};
}

Timestamp Packet::dts() const
{
    return {raw()->dts, m_timeBase};
}

Timestamp Packet::ts() const
{
    return {raw()->pts != av::NoPts ? raw()->pts : raw()->dts, m_timeBase};
}

size_t Packet::size() const
{
    return raw()->size < 0 ? 0 : (size_t)raw()->size;
}

void Packet::setPts(int64_t pts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        raw()->pts = pts;
    else
        raw()->pts = tsTimeBase.rescale(pts, m_timeBase);
}

void Packet::setDts(int64_t dts, const Rational &tsTimeBase)
{
    if (tsTimeBase == Rational(0,0))
        raw()->dts = dts;
    else
        raw()->dts = tsTimeBase.rescale(dts, m_timeBase);
}

void Packet::setPts(const Timestamp &pts)
{
    if (m_timeBase == Rational())
        m_timeBase = pts.timebase();
    raw()->pts = pts.timestamp(m_timeBase);
}

void Packet::setDts(const Timestamp &dts)
{
    if (m_timeBase == Rational())
        m_timeBase = dts.timebase();
    raw()->dts = dts.timestamp(m_timeBase);
}

int Packet::streamIndex() const
{
    return  raw()->stream_index;
}

int Packet::flags() const
{
    return raw()->flags;
}

bool Packet::isKeyPacket() const
{
    return (raw()->flags & AV_PKT_FLAG_KEY);
}

int Packet::duration() const
{
    return raw()->duration;
}

bool Packet::isComplete() const
{
    return m_completeFlag && raw()->data && raw()->size;
}

bool Packet::isNull() const
{
    return raw()->data == nullptr || raw()->size == 0;
}

void Packet::setStreamIndex(int idx)
{
    raw()->stream_index = idx;
}

void Packet::setKeyPacket(bool keyPacket)
{
    if (keyPacket)
        raw()->flags |= AV_PKT_FLAG_KEY;
    else
        raw()->flags &= ~AV_PKT_FLAG_KEY;
}

void Packet::setFlags(int flags)
{
    raw()->flags = flags;
}

void Packet::addFlags(int flags)
{
    raw()->flags |= flags;
}

void Packet::clearFlags(int flags)
{
    raw()->flags &= ~flags;
}

void Packet::dump(const Stream &st, bool dumpPayload) const
{
    if (!st.isNull())
    {
        const AVStream *stream = st.raw();
        av_pkt_dump2(stdout, raw(), dumpPayload ? 1 : 0, stream);
    }
}

void Packet::setTimeBase(const Rational &tb)
{
    if (m_timeBase == tb)
        return;

    if (m_timeBase != Rational())
        av_packet_rescale_ts(raw(),
                             m_timeBase.getValue(),
                             tb.getValue());

    m_timeBase = tb;
}

#ifdef AVCPP_HAS_PKT_SIDE_DATA
std::span<const uint8_t> Packet::sideData(AVPacketSideDataType type) const
{
    std::size_t size;
    auto const data = av_packet_get_side_data(raw(), type, &size);
    return data ? std::span<const uint8_t>{} : std::span<const uint8_t>{data, size};
}

std::span<uint8_t> Packet::sideData(AVPacketSideDataType type)
{
    std::size_t size;
    auto const data = av_packet_get_side_data(raw(), type, &size);
    return data ? std::span<uint8_t>{} : std::span<uint8_t>{data, size};
}

PacketSideData Packet::sideData(std::size_t index) noexcept
{
    if (!m_raw)
        return {};
    if (index >= sideDataElements())
        return {};
    return PacketSideData{m_raw->side_data[index]};
}

ArrayView<AVPacketSideData, PacketSideData, std::size_t> Packet::sideData() noexcept
{
    return make_array_view_size<PacketSideData>(m_raw->side_data, m_raw->side_data_elems);
}

ArrayView<const AVPacketSideData, PacketSideData, std::size_t> Packet::sideData() const noexcept
{
    return make_array_view_size<PacketSideData>((const AVPacketSideData*)m_raw->side_data, m_raw->side_data_elems);
}

size_t Packet::sideDataElements() const noexcept
{
    return m_raw ? m_raw->side_data_elems : 0;
}

void Packet::addSideData(AVPacketSideDataType type, std::span<const uint8_t> data, OptionalErrorCode ec)
{
    clear_if(ec);
    auto newdata = av::memdup<uint8_t>(data.data(), data.size());
    if (!newdata) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return;
    }
    if (auto ret = av_packet_add_side_data(raw(), type, newdata.get(), data.size()); ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
    }
    newdata.release();
}

void Packet::addSideData(AVPacketSideDataType type, std::span<uint8_t> data, wrap_data, OptionalErrorCode ec)
{
    clear_if(ec);
    if (auto ret = av_packet_add_side_data(raw(), type, data.data(), data.size()); ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
    }
}

std::span<uint8_t> Packet::allocateSideData(AVPacketSideDataType type, std::size_t size, OptionalErrorCode ec)
{
    clear_if(ec);
    auto data = av_packet_new_side_data(raw(), type, size);
    if (!data) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }
    // data owned by the packet itself
    return {data, size};
}
#endif

bool Packet::isReferenced() const
{
    return raw()->buf;
}

int Packet::refCount() const
{
    if (raw()->buf)
        return av_buffer_get_ref_count(raw()->buf);
    else
        return 0;
}

#if API_AVCODEC_NEW_INIT_PACKET
AVPacket *Packet::makeRef(OptionalErrorCode ec) const
{
    clear_if(ec);
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return nullptr;
    }

    auto const sts = av_packet_ref(pkt, raw());
    if (sts < 0) {
        av_packet_free(&pkt);
        throws_if(ec, sts, ffmpeg_category());
        return nullptr;
    }

    return pkt;
}

#else

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
#endif

Packet Packet::clone(OptionalErrorCode ec) const
{
    Packet pkt;
    pkt.initFromAVPacket(raw(), true, ec);
    pkt.m_timeBase = m_timeBase;
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

#if API_AVCODEC_NEW_INIT_PACKET
Packet &Packet::operator=(const AVPacket *rhs)
{
    if (rhs == raw())
        return *this;

    initFromAVPacket(rhs, false, throws());
    m_timeBase = Rational();
    return *this;
}
#else
Packet &Packet::operator=(const AVPacket &rhs)
{
    if (&rhs == raw())
        return *this;

    initFromAVPacket(&rhs, false, throws());
    m_timeBase = Rational();
    return *this;
}
#endif

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
        raw()->duration = duration;
    else
        raw()->duration = durationTimeBase.rescale(duration, m_timeBase);
}

#ifdef AVCPP_HAS_PKT_SIDE_DATA
string_view PacketSideData::name() const noexcept
{
    return name(m_raw.type);
}

AVPacketSideDataType PacketSideData::type() const noexcept
{
    return m_raw.type;
}

std::span<const uint8_t> PacketSideData::data() const noexcept
{
    return {m_raw.data, m_raw.size};
}

std::span<uint8_t> PacketSideData::data() noexcept
{
    return {m_raw.data, m_raw.size};
}

bool PacketSideData::empty() const noexcept
{
    return !m_raw.data || !m_raw.size;
}

PacketSideData::operator bool() const noexcept
{
    return !empty();
}

string_view PacketSideData::name(AVPacketSideDataType type)
{
    auto nm = av_packet_side_data_name(type);
    return nm ? std::string_view{nm} : std::string_view{};
}
#endif

} // ::av
