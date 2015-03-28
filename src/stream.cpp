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

bool Stream2::isAudio() const
{
    return (mediaType() == AVMEDIA_TYPE_AUDIO);
}

bool Stream2::isVideo() const
{
    return (mediaType() == AVMEDIA_TYPE_VIDEO);
}

bool Stream2::isData() const
{
    return (mediaType() == AVMEDIA_TYPE_DATA);
}

bool Stream2::isSubtitle() const
{
    return (mediaType() == AVMEDIA_TYPE_SUBTITLE);
}

bool Stream2::isAttachment() const
{
    return (mediaType() == AVMEDIA_TYPE_ATTACHMENT);
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

} // ::av

