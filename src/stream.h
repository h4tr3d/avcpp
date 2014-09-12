#ifndef AV_STREAM_H
#define AV_STREAM_H

#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "container.h"

namespace av
{

enum class Direction
{
    INVALID = -1,
    ENCODING,
    DECODING
};


class Stream2 : public FFWrapperPtr<AVStream>
{
private:
    friend class FormatContext;
    Stream2(const std::shared_ptr<char> &monitor, AVStream *st = nullptr, Direction direction = Direction::INVALID);

public:
    Stream2() = default;

    bool isValid() const;

    int index() const;
    int id() const;

    Rational    frameRate()          const;
    Rational    timeBase()           const;
    Rational    sampleAspectRatio()  const;
    int64_t     startTime()          const;
    int64_t     duration()           const;
    int64_t     currentDts()         const;
    AVMediaType mediaType()          const;

    Direction   direction() const { return m_direction; }

    void setTimeBase(const Rational &timeBase);
    void setFrameRate(const Rational &frameRate);
    void setSampleAspectRatio(const Rational &aspectRatio);

private:
    std::weak_ptr<char> m_parentMonitor;
    Direction           m_direction = Direction::INVALID;
};

// Forward decl
typedef std::shared_ptr<class Container> ContainerPtr;
typedef std::weak_ptr<class Container> ContainerWPtr;

typedef std::shared_ptr<class Stream> StreamPtr;
typedef std::weak_ptr<class Stream> StreamWPtr;

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
