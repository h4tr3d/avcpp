#ifndef CONTAINER_H
#define CONTAINER_H

#include <iostream>
#include <vector>

#include <thread>
#include <functional>
#include <memory>
#include <chrono>


#include "ffmpeg.h"
#include "stream.h"
#include "containerformat.h"
#include "packet.h"
#include "codec.h"

namespace av
{

class Container;
typedef std::shared_ptr<Container> ContainerPtr;
typedef std::weak_ptr<Container> ContainerWPtr;


class Stream;
typedef std::shared_ptr<Stream> StreamPtr;
typedef std::weak_ptr<Stream> StreamWPtr;


class Packet;
typedef std::shared_ptr<Packet> PacketPtr;
typedef std::weak_ptr<Packet> PacketWPtr;

struct AbstractWriteFunctor
{
public:
    virtual int operator() (uint8_t *buf, int size) = 0;
    virtual const char *name() const = 0;
};


// private data
struct ContainerPriv;

class Container : public std::enable_shared_from_this<Container>
{
    friend int avioInterruptCallback(void *ptr);

public:
    Container();
    Container(const ContainerFormatPtr &containerFormat);
    virtual ~Container();


    bool openInput(const char *uri, const ContainerFormatPtr &inputFormat = ContainerFormatPtr(), AVDictionary **options = 0);
    int32_t readNextPacket(PacketPtr &pkt);

    void setReadingTimeout(int64_t value);
    int64_t getReadingTimeout() const;


    const StreamPtr addNewStream(const CodecPtr& codec);
    bool openOutput(const char *uri);
    bool openOutput(const AbstractWriteFunctor &writer);

    bool writeHeader();
    int writePacket(const PacketPtr &packet, bool interleaveWrite = true);
    int writeTrailer();


    void flush();
    void close();
    bool isOpened() const;
    int  getStreamsCount() const { return streams.size(); }
    const StreamPtr& getStream(uint32_t index);

    void setFormat(const ContainerFormatPtr &newFormat);
    const ContainerFormatPtr& getFormat() { return format; }

    AVFormatContext *getAVFromatContext();

    void setIndex(uint index);
    uint getIndex() const;

    void dump();

private:
    void setupInterruptHandling();
    int  avioInterruptHandler();

    void init();
    void reset();

    void queryInputStreams();
    void setupInputStreams();

private:
    AVFormatContext    *context;
    ContainerFormatPtr  format;
    std::vector<StreamPtr>   streams;

    std::string              uri;

    std::chrono::time_point<std::chrono::system_clock> lastStartReadFrameTime;
    int64_t     readingTimeout;

    ContainerPriv       *priv;
};


} // ::av

#endif // CONTAINER_H
