#ifndef PACKET_H
#define PACKET_H

#include <iostream>
#include <vector>

#include "ffmpeg.h"
#include "rational.h"
#include "stream.h"

namespace av {

using namespace std;


class Stream;
typedef boost::shared_ptr<Stream> StreamPtr;
typedef boost::weak_ptr<Stream> StreamWPtr;


class Packet;
typedef boost::shared_ptr<Packet> PacketPtr;
typedef boost::weak_ptr<Packet> PacketWPtr;


class Packet
{
private:
    void ctorInitCommon();
    void ctorInitFromAVPacket(const AVPacket *avpacket);


public:
    Packet();
    Packet(const Packet &packet);
    explicit Packet(const AVPacket *packet);
    explicit Packet(const vector<uint8_t> &data);
    Packet(const uint8_t *data, size_t size, bool doAllign = true);
    ~Packet();

    bool setData(const vector<uint8_t> &newData);
    bool setData(const uint8_t *newData, size_t size);
    const uint8_t* getData() const { return pkt ? pkt->data : 0; }
    AVPacket *getAVPacket() const { return pkt; }

    int64_t getPts() const;
    int64_t getDts() const;
    int64_t getFakePts() const;
    int32_t getSize() const;

    /**
     * Set packet PTS field. It also set fake pts value, so, if you need fake value, you should
     * store it before and restore later. It useful for audio samples: PTS and DTS values in
     * encoded packets in any cases have AV_NOPTS_VALUE!
     *
     * @param pts new presentation timestamp value
     */
    void setPts(int64_t pts);
    void setDts(int64_t dts);
    void setFakePts(int64_t pts);

    int     getStreamIndex() const;
    bool    isKeyPacket() const;
    int     getDuration() const;
    bool    isComplete() const;

    void    setStreamIndex(int idx);
    void    setKeyPacket(bool keyPacket);
    void    setDuration(int duration);
    void    setComplete(bool complete);

    // Flags
    int32_t     getFlags();
    void        setFlags(int32_t flags);
    void        addFlags(int32_t flags);
    void        clearFlags(int32_t flags);

    void        dump(const StreamPtr &st, bool dumpPayload = false) const;

    const Rational& getTimeBase() const { return timeBase; }
    void setTimeBase(const Rational &value);

    Packet& operator= (const Packet &newPkt);
    Packet& operator= (const AVPacket &newPkt);

private:
    int allocatePayload(int32_t   size);
    int reallocatePayload(int32_t newSize);

private:
    AVPacket        *pkt;
    bool             isCompleteFlag;
    Rational         timeBase;
    int64_t          fakePts;
};



} // ::av


#endif // PACKET_H
