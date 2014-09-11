#include <cassert>
#include <cstdio>
#include <iostream>

#include "audiosamples.h"

using namespace std;

namespace av {

AudioSamples::AudioSamples()
{
}

AudioSamples::AudioSamples(const AVFrame *frame)
{
    initFromAVFrame(frame);
    m_timeBase = Rational(AV_TIME_BASE_Q);
    m_completeFlag = true;
}

AudioSamples::AudioSamples(const AudioSamples &frame)
{
    initFromAVFrame(frame.getAVFrame());
    m_timeBase     = frame.getTimeBase();
    m_completeFlag = frame.isComplete();
    m_streamIndex  = frame.getStreamIndex();
    m_fakePts      = frame.getFakePts();
}

AudioSamples::AudioSamples(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate)
{
    init(sampleFormat, samplesCount, channels, sampleRate);
    int size = getSize();
    if (size > 0)
        m_frameBuffer.resize(size);
    avcodec_fill_audio_frame(m_frame, channels, sampleFormat, m_frameBuffer.data(), m_frameBuffer.size(), 0);
    m_timeBase = Rational(AV_TIME_BASE_Q);
}

AudioSamples::AudioSamples(const vector<uint8_t> &data, AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate)
{
    init(sampleFormat, samplesCount, channels, sampleRate);
    // TODO: add check for data size
    //int size = getSize();
    m_frameBuffer = data;
    avcodec_fill_audio_frame(m_frame, channels, sampleFormat, m_frameBuffer.data(), m_frameBuffer.size(), 0);
    m_timeBase = Rational(AV_TIME_BASE_Q);
}

AudioSamples::~AudioSamples()
{
}

AVSampleFormat AudioSamples::getSampleFormat() const
{
    return (m_frame ? (AVSampleFormat)m_frame->format : AV_SAMPLE_FMT_NONE);
}

void AudioSamples::setSampleFormat(AVSampleFormat sampleFormat)
{
    if (m_frame)
    {
        m_frame->format = sampleFormat;
    }
}

int AudioSamples::getSamplesCount() const
{
    return (m_frame ? m_frame->nb_samples : -1);
}

void AudioSamples::setSamplesCount(int samples)
{
    if (m_frame)
    {
        m_frame->nb_samples = samples;
    }
}

int AudioSamples::getChannelsCount() const
{
    assert(m_frame);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
    return av_get_channel_layout_nb_channels(getChannelsLayout());
#else
    return av_frame_get_channels(m_frame);
#endif
}

void AudioSamples::setChannelsCount(int channels)
{
    assert(m_frame);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
    setChannelsLayout(av_get_default_channel_layout(channels));
#else
    av_frame_set_channels(m_frame, channels);
#endif
}

int64_t AudioSamples::getChannelsLayout() const
{
    assert(m_frame);

    return av_frame_get_channel_layout(m_frame);
}

void AudioSamples::setChannelsLayout(int64_t channelsLayout)
{
    assert(m_frame);
    av_frame_set_channel_layout(m_frame, channelsLayout);
}

int AudioSamples::getSampleRate() const
{
    return (m_frame ? av_frame_get_sample_rate(m_frame) : -1);
}

void AudioSamples::setSampleRate(int rate)
{
    if (m_frame)
    {
        av_frame_set_sample_rate(m_frame, rate);
    }
}

uint AudioSamples::getSampleBitDepth() const
{
    return (m_frame ? (av_get_bytes_per_sample(static_cast<AVSampleFormat>(m_frame->format))) << 3 : 0);
}

int AudioSamples::getSize() const
{
    if (m_frame &&
        m_frame->format > -1 &&
        m_frame->nb_samples > 0 &&
        getChannelsCount() > 0)
    {
        return av_samples_get_buffer_size(0,
                                          getChannelsCount(),
                                          m_frame->nb_samples,
                                          static_cast<AVSampleFormat>(m_frame->format),
                                          0);
    }

    return -1;
}

bool AudioSamples::isValid() const
{
    if (m_frame &&
        m_frame->format > -1 &&
        m_frame->nb_samples > 0 &&
        getChannelsCount() > 0 &&
        m_frameBuffer.size() == getSize() &&
        m_frameBuffer.data() == m_frame->data[0])
    {
        return true;
    }

    return false;
}

std::shared_ptr<Frame> AudioSamples::clone()
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
        std::fprintf(stderr,
                     "Can't allocate memory for audio sample data: "
                     "empty audio sample (ch:%d, nb_samples:%d, fmt:%d)",
                     getChannelsCount(),
                     frame->nb_samples,
                     frame->format);

        return;
    }

    m_frame->format         = frame->format;
    m_frame->nb_samples     = frame->nb_samples;
    m_frame->channels       = frame->channels;
    m_frame->channel_layout = frame->channel_layout;

    if (m_frameBuffer.size() != (size_t)size)
    {
        m_frameBuffer.resize(size);
    }

    uint8_t* buffer = (uint8_t*)m_frameBuffer.data();
    if (frame->data[0])
    {
        if (buffer != frame->data[0])
        {
            avcodec_fill_audio_frame(this->m_frame, getChannelsCount(), static_cast<AVSampleFormat>(frame->format), buffer, size, 0);
#if 0
            av_samples_copy(&this->m_frame->data[0], &frame->data[0], 0, 0, frame->nb_samples, getChannelsCount(), static_cast<AVSampleFormat>(frame->format));
#endif
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
    m_frame->format      = sampleFormat;
    m_frame->sample_rate = sampleRate;
    m_frame->nb_samples  = samplesCount;

    setChannelsCount(channels);
}

} // namespace av
