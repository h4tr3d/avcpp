#pragma once

/**
 * Compressed packets and their side data.
 *
 * @file
 * @ingroup avformat
 */

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

#if AVCPP_HAS_PKT_SIDE_DATA
/**
 * Simple view for the AVPacketSideData elements
 */
class PacketSideData : public FFWrapperRef<AVPacketSideData>
{
public:
    using FFWrapperRef<AVPacketSideData>::FFWrapperRef;

    /**
     * Name of the side data type, as FFmpeg spells it.
     *
     * @return a view into a static string owned by FFmpeg
     */
    std::string_view name() const noexcept;
    /**
     * Identifier of the side data type.
     *
     * @return the `AV_PKT_DATA_*` value
     */
    AVPacketSideDataType type() const noexcept;
    /**
     * Read-only view of the payload.
     *
     * @return the bytes of this side data entry
     */
    std::span<const uint8_t> span() const noexcept;
    /**
     * Mutable view of the payload.
     *
     * @return the bytes of this side data entry
     */
    std::span<uint8_t> span() noexcept;

    /**
     * Name of a side data type.
     *
     * @param type the `AV_PKT_DATA_*` value
     *
     * @return a view into a static string owned by FFmpeg, or an empty view for
     *         an unknown type
     */
    static std::string_view name(AVPacketSideDataType type);

    /**
     * Whether the entry carries no payload.
     *
     * @return `true` when the payload size is zero
     */
    bool empty() const noexcept;
    /**
     * Whether the entry is present.
     *
     * @return `true` when the entry has a payload
     */
    operator bool() const noexcept;
};
#endif // AVCPP_HAS_PKT_SIDE_DATA

/**
 * Compressed data and its metadata.
 *
 * A packet belongs to exactly one stream: it carries the stream index, the
 * presentation and decoding timestamps, the duration and the flags of one
 * compressed unit. Reading gives packets, writing consumes them; decoding turns
 * them into frames and encoding turns frames back into packets.
 *
 * Buffers are allocated with the FFmpeg allocator and always carry the zeroed
 * AV_INPUT_BUFFER_PADDING_SIZE bytes after the payload. This is not optional:
 * some muxers and parsers read payloads as C strings, and the ASS muxer does
 * exactly that.
 */
class Packet :
#if AVCPP_API_AVCODEC_NEW_INIT_PACKET
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

#if AVCPP_API_AVCODEC_NEW_INIT_PACKET
    /**
     * Create an empty packet without allocating.
     *
     * @note No checks are performed, unlike Packet(); the packet is invalid
     *       until data is set.
     */
    explicit Packet(std::nullptr_t); // explicit nullptr packet. There is no extra internal checks, so be careful
#endif

    /**
     * Create an empty, valid packet with an allocated FFmpeg structure.
     */
    Packet();
    /**
     * Copy a packet as a reference to the same data.
     *
     * @param packet the packet to reference
     * @param ec     holder for the error code
     */
    Packet(const Packet &packet, OptionalErrorCode ec);
    /**
     * Copy a packet as a reference to the same data.
     *
     * @param packet the packet to reference
     */
    Packet(const Packet &packet);
    /**
     * Move a packet.
     *
     * @param packet the packet to move from; left empty
     */
    Packet(Packet &&packet);
    /**
     * Wrap an FFmpeg packet.
     *
     * The data is referenced, not copied.
     *
     * @param packet the packet to wrap; null produces an empty packet
     * @param ec     holder for the error code
     */
    explicit Packet(const AVPacket *packet, OptionalErrorCode ec = throws());
    /**
     * Copy a byte vector into the packet payload.
     *
     * @param data the payload to copy
     */
    explicit Packet(const std::vector<uint8_t> &data);

    /**
     * Copy a memory block into the packet payload.
     *
     * @param data      the payload
     * @param size      payload size in bytes
     * @param doAllign  `true` appends the zeroed padding FFmpeg requires after
     *                  the payload; disabling it is only safe when the buffer
     *                  already provides it
     */
    Packet(const uint8_t *data, size_t size, bool doAllign = true);
    // data must be allocated with av_malloc() family
    /**
     * Take ownership of a buffer allocated with the FFmpeg allocator.
     *
     * @param data the buffer; it must come from av_malloc() or av::malloc()
     * @param size payload size in bytes
     * @param ec   holder for the error code
     *
     * @note The buffer must contain the zeroed padding after the payload,
     *       otherwise allocate `size + AV_INPUT_BUFFER_PADDING_SIZE` bytes.
     */
    Packet(uint8_t *data, size_t size, wrap_data, OptionalErrorCode ec = throws());
    /**
     * Wrap static data without taking ownership.
     *
     * @param data the buffer; it must outlive the packet
     * @param size payload size in bytes
     * @param ec   holder for the error code
     *
     * @note The buffer must be `size + AV_INPUT_BUFFER_PADDING_SIZE` bytes and
     *       the padding must be zeroed.
     */
    Packet(uint8_t *data, size_t size, wrap_data_static, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Copy a span into the packet payload.
     *
     * @param data     the payload
     * @param doAllign `true` appends the zeroed padding FFmpeg requires
     */
    explicit Packet(std::span<const uint8_t> data, bool doAllign = true);
    // data must be allocated with av_malloc() family
    /**
     * Take ownership of a span allocated with the FFmpeg allocator.
     *
     * @param data the buffer; it must come from av_malloc() or av::malloc()
     *             and must contain the zeroed AV_INPUT_BUFFER_PADDING_SIZE
     *             bytes after the payload
     * @param ec   holder for the error code
     */
    Packet(std::span<uint8_t> data, wrap_data, OptionalErrorCode ec = throws());
    /**
     * Wrap static data from a span without taking ownership.
     *
     * @param data the buffer; it must outlive the packet and include the
     *             zeroed FFmpeg padding
     * @param ec   holder for the error code
     */
    Packet(std::span<uint8_t> data, wrap_data_static, OptionalErrorCode ec = throws());
#endif

    ~Packet();

    /**
     * Replace the payload with a copy of a byte vector.
     *
     * @param newData the new payload
     * @param ec      holder for the error code
     *
     * @return `true` when the payload was replaced
     */
    bool setData(const std::vector<uint8_t> &newData, OptionalErrorCode ec = throws());
    /**
     * Replace the payload with a copy of a memory block.
     *
     * The packet allocates `size + AV_INPUT_BUFFER_PADDING_SIZE` bytes and zeroes
     * the padding itself.
     *
     * @param newData the new payload
     * @param size    payload size in bytes
     * @param ec      holder for the error code
     *
     * @return `true` when the payload was replaced
     */
    bool setData(const uint8_t *newData, size_t size, OptionalErrorCode ec = throws());

    /**
     * Read access to the payload.
     *
     * @return the payload pointer, or nullptr for an empty packet
     */
    const uint8_t* data() const;
    /**
     * Write access to the payload.
     *
     * @return the payload pointer, or nullptr for an empty packet
     *
     * @note Writing into the buffer of a shared packet changes the data seen by
     *       every reference to it; call clone() first when that matters.
     */
    uint8_t*       data();

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Replace the payload with a copy of a span.
     *
     * @param newData the new payload
     * @param ec      holder for the error code
     *
     * @return `true` when the payload was replaced
     */
    bool setData(std::span<const uint8_t> newData, OptionalErrorCode ec = throws());
    /**
     * Read-only view of the payload.
     *
     * @return the bytes, without the padding
     */
    std::span<const uint8_t> span() const;
    /**
     * Mutable view of the payload.
     *
     * @return the bytes, without the padding
     */
    std::span<uint8_t> span();
#endif

    /**
     * Presentation timestamp.
     *
     * @return the timestamp in timeBase() units
     */
    Timestamp pts() const;
    /**
     * Decoding timestamp.
     *
     * @return the timestamp in timeBase() units
     */
    Timestamp dts() const;
    /**
     * Timestamp FFmpeg would use for display, falling back to the decoding one.
     *
     * @return `pts` when it is set, otherwise `dts`
     */
    Timestamp ts() const;
    /**
     * Payload size.
     *
     * @return the number of payload bytes, without the padding
     */
    size_t size() const;

    /**
     * Set the presentation timestamp.
     *
     * @deprecated Pass a Timestamp instead, so the time base travels with the
     *             value.
     *
     * @param pts         new presentation timestamp value
     * @param tsTimeBase  time base of the value; the zero value means the packet
     *                    time base
     */
    attribute_deprecated void setPts(int64_t pts,     const Rational &tsTimeBase = Rational(0, 0));
    /**
     * Set the decoding timestamp.
     *
     * @deprecated Pass a Timestamp instead, so the time base travels with the
     *             value.
     *
     * @param dts         new decoding timestamp value
     * @param tsTimeBase  time base of the value; the zero value means the packet
     *                    time base
     */
    attribute_deprecated void setDts(int64_t dts,     const Rational &tsTimeBase = Rational(0, 0));

    /**
     * Set the presentation timestamp.
     *
     * @param pts the timestamp; its time base replaces the stored one
     */
    void setPts(const Timestamp &pts);
    /**
     * Set the decoding timestamp.
     *
     * @param dts the timestamp; its time base replaces the stored one
     */
    void setDts(const Timestamp &dts);

    /**
     * Index of the stream the packet belongs to.
     *
     * @return the stream index
     */
    int     streamIndex() const;
    /**
     * Whether the packet is a key frame.
     *
     * @return the `AV_PKT_FLAG_KEY` state
     */
    bool    isKeyPacket() const;
    /**
     * Duration of the packet, in time base units.
     *
     * @return the duration
     */
    int     duration() const;
    /**
     * Whether the packet is marked complete.
     *
     * @return the completeness flag, set by the reader or by setComplete()
     */
    bool    isComplete() const;
    /**
     * Whether the packet has no payload.
     *
     * @return `true` when the data pointer is null or the size is zero
     */
    bool    isNull() const;

    /**
     * Set the stream index.
     *
     * Must be set before writing the packet into a container, and remapped when
     * input and output streams differ.
     *
     * @param idx index of the stream
     */
    void    setStreamIndex(int idx);
    /**
     * Mark the packet as a key frame.
     *
     * @param keyPacket the new value of the flag
     */
    void    setKeyPacket(bool keyPacket);
    /**
     * Set the packet duration.
     *
     * @param duration         duration in `durationTimeBase` units
     * @param durationTimeBase time base of the value; the default zero value
     *                         means "the packet time base"
     */
    void    setDuration(int duration, const Rational &durationTimeBase = Rational(0, 0));
    /**
     * Mark the packet as complete.
     *
     * @param complete `true` when the payload holds a whole compressed unit
     */
    void    setComplete(bool complete);

    // Flags
    /**
     * Raw `AV_PKT_FLAG_*` mask.
     *
     * @return the flags of the packet
     */
    int         flags() const;
    /**
     * Replace the `AV_PKT_FLAG_*` mask.
     *
     * @param flags the new mask
     */
    void        setFlags(int flags);
    /**
     * Add flags.
     *
     * @param flags the bits to set
     */
    void        addFlags(int flags);
    /**
     * Remove flags.
     *
     * @param flags the bits to clear
     */
    void        clearFlags(int flags);

#if AVCPP_HAS_AVFORMAT
    /**
     * Print the packet through the FFmpeg log system.
     *
     * @param st          the stream the packet belongs to; used to convert
     *                    timestamps to seconds
     * @param dumpPayload `true` also dumps the payload as hex
     */
    void        dump(const Stream & st, bool dumpPayload = false) const;
#endif // if AVCPP_HAS_AVFORMAT

    /**
     * Time base of pts() and dts().
     *
     * @return timestamp units per second
     */
    const Rational& timeBase() const { return m_timeBase; }
    /**
     * Set the time base used by pts() and dts().
     *
     * @param value timestamp units per second
     */
    void setTimeBase(const Rational &value);

#if AVCPP_HAS_PKT_SIDE_DATA
    /**
     * Read-only access to the side data of the given type.
     *
     * @param type the `AV_PKT_DATA_*` value to look for
     *
     * @return the payload, empty when the packet has none
     */
    std::span<const uint8_t> sideData(AVPacketSideDataType type) const;
    /**
     * Mutable access to the side data of the given type.
     *
     * @param type the `AV_PKT_DATA_*` value to look for
     *
     * @return the payload, empty when the packet has none
     */
    std::span<uint8_t>       sideData(AVPacketSideDataType type);

    /**
     * Return count of the side data elements
     * @return
     */
    std::size_t sideDataCount() const noexcept;

    /**
     * Get side data element by index
     * @param index
     * @return
     */
    PacketSideData sideDataIndex(std::size_t index) noexcept;

    /**
     * Observe all packet side data via iterators
     *
     * Applicable for range-based for and std::range:xxx algos.
     *
     * @return
     */
    ArrayView<AVPacketSideData, PacketSideData, std::size_t> sideData() noexcept;
    /**
     * Read-only iteration over all side data entries.
     *
     * @return a non-owning range valid while the packet is alive
     */
    ArrayView<const AVPacketSideData, PacketSideData, std::size_t> sideData() const noexcept;

    /**
     * Remove and free all side data entries.
     */
    void freeSideData() noexcept;

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

    /**
     * Whether the payload is reference counted.
     *
     * @return `true` when the data is shared through AVBufferRef
     */
    bool     isReferenced() const;
    /**
     * Number of references to the payload.
     *
     * @return the reference count, or zero when the packet is not referenced
     */
    int      refCount() const;

#if AVCPP_API_AVCODEC_NEW_INIT_PACKET
    /**
     * Create an independent reference to the packet.
     *
     * @param ec holder for the error code
     *
     * @return a new AVPacket that shares the payload and must be freed by the
     *         caller, or nullptr on failure
     */
    AVPacket* makeRef(OptionalErrorCode ec) const;
#else
    /**
     * Create a reference to the packet as a plain FFmpeg value.
     *
     * @param ec holder for the error code
     *
     * @return a copy that shares the payload
     */
    AVPacket makeRef(OptionalErrorCode ec = throws()) const;
#endif

    /**
     * Deep copy of the packet, payload included.
     *
     * @param ec holder for the error code
     *
     * @return an independent packet, or an empty one on failure
     */
    Packet   clone(OptionalErrorCode ec = throws()) const;

    /**
     * Assign by referencing the other packet's payload.
     *
     * @param rhs the packet to assign from
     *
     * @return this packet
     */
    Packet &operator=(const Packet &rhs);
    /**
     * Move assignment.
     *
     * @param rhs the packet to move from; left empty
     *
     * @return this packet
     */
    Packet &operator=(Packet &&rhs);

#if AVCPP_API_AVCODEC_NEW_INIT_PACKET
    /**
     * Assign from an FFmpeg packet.
     *
     * @param rhs the packet to reference; null empties this one
     *
     * @return this packet
     */
    Packet &operator=(const AVPacket *rhs);
#else
    /**
     * Assign from an FFmpeg packet value.
     *
     * @param rhs the packet to copy
     *
     * @return this packet
     */
    Packet &operator=(const AVPacket &rhs);
#endif

    /**
     * Exchange the contents of two packets.
     *
     * @param other the packet to swap with
     */
    void swap(Packet &other);

    /**
     * Whether the packet is complete.
     *
     * @return the completeness flag, as returned by isComplete()
     */
    operator bool() const { return isComplete(); }

private:
    bool     m_completeFlag;
    Rational m_timeBase;
};



} // ::av

