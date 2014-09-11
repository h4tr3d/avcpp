#include "formatcontext.h"
#include "stream.h"

namespace av
{

Stream2::Stream2(const std::shared_ptr<char> &monitor, AVStream *st, Direction direction)
    : FFWrapperPtr<AVStream>(st),
      m_parentMonitor(monitor),
      m_direction(direction)
{
}

bool Stream2::isValid() const
{
    return (!m_parentMonitor.expired() && !isNull());
}

int Stream2::index() const
{
    return RAW_GET2(isValid(), index, -1);
}

int Stream2::id() const
{
    return RAW_GET2(isValid(), id, -1);
}

Rational Stream2::frameRate() const
{
    return RAW_GET2(isValid(), r_frame_rate, Rational());
}

Rational Stream2::timeBase() const
{
    return RAW_GET2(isValid(), time_base, Rational());
}

Rational Stream2::sampleAspectRatio() const
{
    return RAW_GET2(isValid(), sample_aspect_ratio, Rational());
}

int64_t Stream2::startTime() const
{
    return RAW_GET2(isValid(), start_time, AV_NOPTS_VALUE);
}

int64_t Stream2::duration() const
{
    return RAW_GET2(isValid(), duration, AV_NOPTS_VALUE);
}

int64_t Stream2::currentDts() const
{
    return RAW_GET2(isValid(), cur_dts, AV_NOPTS_VALUE);
}

AVMediaType Stream2::mediaType() const
{
    return RAW_GET2(isValid() && m_raw->codec, codec->codec_type, AVMEDIA_TYPE_UNKNOWN);
}

void Stream2::setTimeBase(const Rational &timeBase)
{
    RAW_SET2(isValid(), time_base, timeBase.getValue());
}

void Stream2::setFrameRate(const Rational &frameRate)
{
    RAW_SET2(isValid(), r_frame_rate, frameRate.getValue());
}

void Stream2::setSampleAspectRatio(const Rational &aspectRatio)
{
    RAW_SET2(isValid(), sample_aspect_ratio, aspectRatio.getValue());
}


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

