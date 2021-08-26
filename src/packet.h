#ifndef PACKET_H
#define PACKET_H

#include <iostream>
#include <vector>

#include "ffmpeg.h"
#include "rational.h"
#include "stream.h"
#include "averror.h"
#include "timestamp.h"

extern "C" {
#include <libavutil/attributes.h>
}

namespace av {

class Packet :
#if DEPRECATED_INIT_PACKET
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
    ~Packet();

    bool setData(const std::vector<uint8_t> &newData, OptionalErrorCode ec = throws());
    bool setData(const uint8_t *newData, size_t size, OptionalErrorCode ec = throws());

    const uint8_t* data() const;
    uint8_t*       data();

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

    bool     isReferenced() const;
    int      refCount() const;

#if DEPRECATED_INIT_PACKET
    AVPacket* makeRef(OptionalErrorCode ec) const;
#else
    AVPacket makeRef(OptionalErrorCode ec = throws()) const;
#endif

    Packet   clone(OptionalErrorCode ec = throws()) const;

    Packet &operator=(const Packet &rhs);
    Packet &operator=(Packet &&rhs);

#if DEPRECATED_INIT_PACKET
    Packet &operator=(const AVPacket *rhs);
#else
    Packet &operator=(const AVPacket &rhs);
#endif

    void swap(Packet &other);

    operator bool() const { return isComplete(); }

private:
#if 0
    int allocatePayload(int32_t   size);
    int reallocatePayload(int32_t newSize);
#endif

private:
    bool     m_completeFlag;
    Rational m_timeBase;
};



} // ::av


#endif // PACKET_H
