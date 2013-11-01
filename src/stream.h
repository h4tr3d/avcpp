#ifndef STREAM_H
#define STREAM_H

#include <boost/smart_ptr.hpp>

#include "ffmpeg.h"
#include "rational.h"
#include "container.h"

namespace av
{

// Forward decl
class Container;
typedef boost::shared_ptr<Container> ContainerPtr;
typedef boost::weak_ptr<Container> ContainerWPtr;


class Stream;
typedef boost::shared_ptr<Stream> StreamPtr;
typedef boost::weak_ptr<Stream> StreamWPtr;


typedef enum
{
    INVALID = -1,
    ENCODING,
    DECODING
} Direction;



class Stream
{
public:
    Stream(const ContainerPtr &container, AVStream *stream, Direction direction, AVCodec *codec);

    const AVStream *getAVStream() const { return stream; }
    AVStream *getAVStream() { return stream; }

    int getIndex();
    int getId();

    Rational    getFrameRate()          const;
    Rational    getTimeBase()           const;
    Rational    getSampleAspectRatio() const;
    int64_t     getStartTime()          const;
    int64_t     getDuration()           const;
    int64_t     getCurrentDts()         const;
    int         getStreamIndex()        const;
    AVMediaType getMediaType()    const;

    void setTimeBase(const Rational &timeBase);
    void setFrameRate(const Rational &frameRate);
    void setSampleAspectRatio(const Rational &aspectRatio);

    Direction getDirection() { return direction; }

protected:
    Stream();

private:
    AVStream *stream;

    ContainerWPtr container;
    Direction     direction;
};


} // ::av

#endif // STREAM_H
