#include "stream.h"

namespace av
{

Stream::Stream()
    : stream(0),
      direction(INVALID)
{
}

Stream::Stream(const ContainerPtr &container, AVStream *stream, Direction direction, AVCodec *codec)
{
    this->container = container;
    this->direction = direction;
    this->stream    = stream;

    // TODO: need to complete, like point StreamCoder
}

Rational Stream::getFrameRate() const
{
    if (stream)
        return stream->r_frame_rate;

    return Rational();
}

Rational Stream::getTimeBase() const
{
    if (stream)
        return stream->time_base;

    return Rational();
}

Rational Stream::getSampleAspectRatio() const
{
    return (stream ? stream->sample_aspect_ratio : Rational());
}

int64_t Stream::getStartTime() const
{
    return (stream ? stream->start_time : AV_NOPTS_VALUE);
}

int64_t Stream::getDuration() const
{
    return (stream ? stream->duration : AV_NOPTS_VALUE);
}

int64_t Stream::getCurrentDts() const
{
    return (stream ? stream->cur_dts : AV_NOPTS_VALUE);
}

int Stream::getStreamIndex() const
{
    return (stream ? stream->index : -1);
}

AVMediaType Stream::getMediaType() const
{
    return (stream && stream->codec ? stream->codec->codec_type : AVMEDIA_TYPE_UNKNOWN);
}

void Stream::setTimeBase(const Rational &timeBase)
{
    if (stream)
        stream->time_base = timeBase.getValue();
}

void Stream::setFrameRate(const Rational &frameRate)
{
    if (stream)
        stream->r_frame_rate = frameRate.getValue();
}

void Stream::setSampleAspectRatio(const Rational &aspectRatio)
{
    if (stream)
        stream->sample_aspect_ratio = aspectRatio.getValue();
}


int Stream::getIndex()
{
    return (stream ? stream->index : -1);
}

int Stream::getId()
{
    return (stream ? stream->id : -1);
}


} // ::av

