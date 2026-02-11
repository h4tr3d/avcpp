#pragma once

#include "avconfig.h"
#include "avcompat.h"

#include <vector>
#include <type_traits>

#if AVCPP_CXX_STANDARD >= 20
#include <span>
#endif

#include "ffmpeg.h"
#include "avutils.h"
#include "rational.h"
#include "stream.h"
#include "averror.h"
#include "timestamp.h"

extern "C" {
#include <libavutil/attributes.h>
}

namespace av {

#ifdef AVCPP_HAS_PKT_SIDE_DATA
/**
 * Simple view for the AVPacketSideData elements
 */
class PacketSideData : public FFWrapperRef<AVPacketSideData>
{
public:
    using FFWrapperRef<AVPacketSideData>::FFWrapperRef;

    std::string_view name() const noexcept;
    AVPacketSideDataType type() const noexcept;
    std::span<const uint8_t> data() const noexcept;
    std::span<uint8_t> data() noexcept;

    static std::string_view name(AVPacketSideDataType type);

    bool empty() const noexcept;
    operator bool() const noexcept;
};
#endif // AVCPP_HAS_PKT_SIDE_DATA

class Packet :
#if API_AVCODEC_NEW_INIT_PACKET
    public FFWrapperPtr<AVPacket>
#else
    public FFWrapperRef<AVPacket>
#endif
{
private:
    // if deepCopy true - make deep copy, instead - reference is created
    void initFromAVPacket(const AVPacket *avpacket, bool deepCopy, OptionalErrorCode ec);

public:
    /**
     * Wrap data and take owning. Data must be allocated with av_malloc() family
     */
    struct wrap_data {};
    /**
     * Wrap static data, do not owning and free.
     * Buffer size must be: size + AV_INPUT_BUFFER_PADDING_SIZE
     */
    struct wrap_data_static {};

    Packet();
    Packet(const Packet &packet, OptionalErrorCode ec);
    Packet(const Packet &packet);
    Packet(Packet &&packet);
    explicit Packet(const AVPacket *packet, OptionalErrorCode ec = throws());
    explicit Packet(const std::vector<uint8_t> &data);

    Packet(const uint8_t *data, size_t size, bool doAllign = true);
    // data must be allocated with av_malloc() family
    Packet(uint8_t *data, size_t size, wrap_data, OptionalErrorCode ec = throws());
    Packet(uint8_t *data, size_t size, wrap_data_static, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    explicit Packet(std::span<const uint8_t> data, bool doAllign = true);
    // data must be allocated with av_malloc() family
    Packet(std::span<uint8_t> data, wrap_data, OptionalErrorCode ec = throws());
    Packet(std::span<uint8_t> data, wrap_data_static, OptionalErrorCode ec = throws());
#endif

    ~Packet();

    bool setData(const std::vector<uint8_t> &newData, OptionalErrorCode ec = throws());
    bool setData(const uint8_t *newData, size_t size, OptionalErrorCode ec = throws());

    const uint8_t* data() const;
    uint8_t*       data();

#if AVCPP_CXX_STANDARD >= 20
    bool setData(std::span<const uint8_t> newData, OptionalErrorCode ec = throws());
    std::span<const uint8_t> span() const;
    std::span<uint8_t> span();
#endif

    Timestamp pts() const;
    Timestamp dts() const;
    Timestamp ts() const;
    size_t size() const;

    /**
     * Set packet PTS field.
     *
     * @param pts new presentation timestamp value
     * @param tsTimeBase  is a time base of setted timestamp, can be omited or sets to Rational(0,0)
     *                    that means that time base equal to packet time base.
     */
    attribute_deprecated void setPts(int64_t pts,     const Rational &tsTimeBase = Rational(0, 0));
    attribute_deprecated void setDts(int64_t dts,     const Rational &tsTimeBase = Rational(0, 0));

    void setPts(const Timestamp &pts);
    void setDts(const Timestamp &dts);

    int     streamIndex() const;
    bool    isKeyPacket() const;
    int     duration() const;
    bool    isComplete() const;
    bool    isNull() const;

    void    setStreamIndex(int idx);
    void    setKeyPacket(bool keyPacket);
    void    setDuration(int duration, const Rational &durationTimeBase = Rational(0, 0));
    void    setComplete(bool complete);

    // Flags
    int         flags() const;
    void        setFlags(int flags);
    void        addFlags(int flags);
    void        clearFlags(int flags);

    void        dump(const Stream & st, bool dumpPayload = false) const;

    const Rational& timeBase() const { return m_timeBase; }
    void setTimeBase(const Rational &value);

#ifdef AVCPP_HAS_PKT_SIDE_DATA
    /**
     * Get packet side data of the given type. Empty buffer means no data.
     *
     * @param type
     * @return
     */
    std::span<const uint8_t> sideData(AVPacketSideDataType type) const;
    std::span<uint8_t>       sideData(AVPacketSideDataType type);

    /**
     * Return count of the side data elements
     * @return
     */
    std::size_t sideDataElements() const noexcept;

    /**
     * Get side data element by index
     * @param index
     * @return
     */
    PacketSideData sideData(std::size_t index) noexcept;

    /**
     * Observe all packet side data via iterators
     *
     * Applicable for range-based for and std::range:xxx algos.
     *
     * @return
     */
    ArrayView<AVPacketSideData, PacketSideData, std::size_t> sideData() noexcept;
    ArrayView<const AVPacketSideData, PacketSideData, std::size_t> sideData() const noexcept;

    /**
     * Add side data of the given type into packet. Data will be cloned.
     * @param type
     * @param data
     * @param ec
     */
    void addSideData(AVPacketSideDataType type, std::span<const uint8_t> data, OptionalErrorCode ec = throws());

    /**
     * Add side data of the given type into packet. Data will be wrapped and owner-shipping taken. Data must be allocated
     * via av_malloc()/av::malloc() family functions.
     *
     * @param type
     * @param data
     * @param ec
     */
    void addSideData(AVPacketSideDataType type, std::span<uint8_t> data, wrap_data, OptionalErrorCode ec = throws());

    /**
     * Allocate storage for the packet side data of the given type and return reference to it. Data owned by the packet.
     * @param type
     * @param size
     * @param ec
     * @return
     */
    std::span<uint8_t> allocateSideData(AVPacketSideDataType type, std::size_t size, OptionalErrorCode ec = throws());
#endif

    bool     isReferenced() const;
    int      refCount() const;

#if API_AVCODEC_NEW_INIT_PACKET
    AVPacket* makeRef(OptionalErrorCode ec) const;
#else
    AVPacket makeRef(OptionalErrorCode ec = throws()) const;
#endif

    Packet   clone(OptionalErrorCode ec = throws()) const;

    Packet &operator=(const Packet &rhs);
    Packet &operator=(Packet &&rhs);

#if API_AVCODEC_NEW_INIT_PACKET
    Packet &operator=(const AVPacket *rhs);
#else
    Packet &operator=(const AVPacket &rhs);
#endif

    void swap(Packet &other);

    operator bool() const { return isComplete(); }

private:
    bool     m_completeFlag;
    Rational m_timeBase;
};



} // ::av

