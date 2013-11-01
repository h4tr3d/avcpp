#include <iostream>

#include <boost/format.hpp>

#include "audiosamples.h"

namespace av {

AudioSamples::AudioSamples()
{
}

AudioSamples::AudioSamples(const AVFrame *frame)
{
    initFromAVFrame(frame);
    timeBase = Rational(AV_TIME_BASE_Q);
    isCompleteFlag = true;
}

AudioSamples::AudioSamples(const AudioSamples &frame)
{
    initFromAVFrame(frame.getAVFrame());
    timeBase       = frame.getTimeBase();
    isCompleteFlag = frame.isComplete();
    streamIndex    = frame.getStreamIndex();
    fakePts        = frame.getFakePts();
}

AudioSamples::AudioSamples(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate)
{
    init(sampleFormat, samplesCount, channels, sampleRate);
    int size = getSize();
    if (size > 0)
        frameBuffer.resize(size);
    avcodec_fill_audio_frame(frame, channels, sampleFormat, frameBuffer.data(), frameBuffer.size(), 0);
    timeBase = Rational(AV_TIME_BASE_Q);
}

AudioSamples::AudioSamples(const vector<uint8_t> &data, AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate)
{
    init(sampleFormat, samplesCount, channels, sampleRate);
    // TODO: add check for data size
    //int size = getSize();
    frameBuffer = data;
    avcodec_fill_audio_frame(frame, channels, sampleFormat, frameBuffer.data(), frameBuffer.size(), 0);
    timeBase = Rational(AV_TIME_BASE_Q);
}

AudioSamples::~AudioSamples()
{
}

AVSampleFormat AudioSamples::getSampleFormat() const
{
    return (frame ? (AVSampleFormat)frame->format : AV_SAMPLE_FMT_NONE);
}

void AudioSamples::setSampleFormat(AVSampleFormat sampleFormat)
{
    if (frame)
    {
        frame->format = sampleFormat;
    }
}

int AudioSamples::getSamplesCount() const
{
    return (frame ? frame->nb_samples : -1);
}

void AudioSamples::setSamplesCount(int samples)
{
    if (frame)
    {
        frame->nb_samples = samples;
    }
}

int AudioSamples::getChannelsCount() const
{
    assert(frame);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
    return av_get_channel_layout_nb_channels(getChannelsLayout());
#else
    return av_frame_get_channels(frame);
#endif
}

void AudioSamples::setChannelsCount(int channels)
{
    assert(frame);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
    setChannelsLayout(av_get_default_channel_layout(channels));
#else
    av_frame_set_channels(frame, channels);
#endif
}

int64_t AudioSamples::getChannelsLayout() const
{
    assert(frame);

    return av_frame_get_channel_layout(frame);
}

void AudioSamples::setChannelsLayout(int64_t channelsLayout)
{
    assert(frame);
    av_frame_set_channel_layout(frame, channelsLayout);
}

int AudioSamples::getSampleRate() const
{
    return (frame ? av_frame_get_sample_rate(frame) : -1);
}

void AudioSamples::setSampleRate(int rate)
{
    if (frame)
    {
        av_frame_set_sample_rate(frame, rate);
    }
}

uint AudioSamples::getSampleBitDepth() const
{
    return (frame ? (av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format))) << 3 : 0);
}

int AudioSamples::getSize() const
{
    if (frame &&
        frame->format > -1 &&
        frame->nb_samples > 0 &&
        getChannelsCount() > 0)
    {
        return av_samples_get_buffer_size(0,
                                          getChannelsCount(),
                                          frame->nb_samples,
                                          static_cast<AVSampleFormat>(frame->format),
                                          0);
    }

    return -1;
}

bool AudioSamples::isValid() const
{
    if (frame &&
        frame->format > -1 &&
        frame->nb_samples > 0 &&
        getChannelsCount() > 0 &&
        frameBuffer.size() == getSize() &&
        frameBuffer.data() == frame->data[0])
    {
        return true;
    }

    return false;
}

boost::shared_ptr<Frame> AudioSamples::clone()
{
    FramePtr result(new AudioSamples(*this));
    return result;
}

void AudioSamples::setPts(int64_t pts)
{
    // Only Set fake pts for audio samples
    setFakePts(pts);
}

int64_t AudioSamples::samplesToTimeDuration(int64_t samplesCount, int sampleRate, const Rational timeBase)
{
    if (samplesCount == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;

    Rational rate(1, sampleRate);

    if (rate == timeBase)
        return samplesCount;

    return rate.rescale(samplesCount, timeBase);
}

int64_t AudioSamples::timeDurationToSamples(int64_t duration, int sampleRate, const Rational timeBase)
{
    if (duration <= 0)
        return AV_NOPTS_VALUE;

    Rational rate(1, sampleRate);

    if (rate == timeBase)
        return duration;

    return timeBase.rescale(duration, rate);
}

void AudioSamples::setupDataPointers(const AVFrame *frame)
{
    ssize_t size = -1;
    if (frame && frame->format > -1 && frame->nb_samples > 0 && getChannelsCount() > 0)
    {
        size = av_samples_get_buffer_size(0,
                                          getChannelsCount(),
                                          frame->nb_samples,
                                          static_cast<AVSampleFormat>(frame->format),
                                          0);
    }

    // This is error....
    if (size < 0)
    {
        std::cout << boost::format("Can't allocate memory for audio sample data: "
                                   "empty audio sample (ch:%d, nb_samples:%d, fmt:%d)") %
                     getChannelsCount() %
                     frame->nb_samples %
                     frame->format
                  << std::endl;
        return;
    }

    if (frameBuffer.size() != (size_t)size)
    {
        frameBuffer.resize(size);
    }

    uint8_t* buffer = (uint8_t*)frameBuffer.data();
    if (frame->data[0])
    {
        if (buffer != frame->data[0])
        {
            avcodec_fill_audio_frame(this->frame, getChannelsCount(), static_cast<AVSampleFormat>(frame->format), buffer, size, 0);
            av_samples_copy(&this->frame->data[0], &frame->data[0], 0, 0, frame->nb_samples, getChannelsCount(), static_cast<AVSampleFormat>(frame->format));
        }
    }
    else
    {
        // Error...
        std::cout << "Can't copy data from frame: src frame has no data to copy\n";
        return;
    }
}

void AudioSamples::init(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate)
{
    frame->format      = sampleFormat;
    frame->sample_rate = sampleRate;
    frame->nb_samples  = samplesCount;

    setChannelsCount(channels);
}

} // namespace av
