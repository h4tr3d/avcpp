#ifndef AV_AUDIOSAMPLES_H
#define AV_AUDIOSAMPLES_H

#include <memory>

#include "frame.h"

namespace av {

class AudioSamples : public Frame
{
public:
    AudioSamples();
    AudioSamples(const AVFrame     *frame);
    AudioSamples(const AudioSamples &frame);
    AudioSamples(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate);
    AudioSamples(const vector<uint8_t> &data,
                AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate);
    virtual ~AudioSamples();

    // Audio specific methods
    AVSampleFormat getSampleFormat() const;
    void           setSampleFormat(AVSampleFormat sampleFormat);

    int            getSamplesCount() const;
    void           setSamplesCount(int samples);

    int            getChannelsCount() const;
    void           setChannelsCount(int channels);

    int64_t        getChannelsLayout() const;
    void           setChannelsLayout(int64_t channelsLayout);

    int            getSampleRate() const;
    void           setSampleRate(int rate);

    uint           getSampleBitDepth() const;

    // public virtual
    virtual int getSize() const;
    virtual bool isValid() const;
    virtual std::shared_ptr<Frame> clone();

    // Common
    // Override this methods: we want to have access to PTS but don't need change it in AVFrame
    // for audio samples
    virtual void  setPts(int64_t pts);


    /**
     * Converts a number of samples at a given sampleRate into time duration in given time base.
     * @param samplesCount Number of samples.
     * @param sampleRate   sample rate that those samples are recorded at.
     * @return duration in given timebase it would take to play that audio.
     */
    static int64_t samplesToTimeDuration(int64_t samplesCount, int sampleRate, const Rational timeBase = Rational(1, 1000000));

    /**
     * Converts a duration in give time base into a number of samples, assuming a given sampleRate.
     * @param duration   The duration in microseconds.
     * @param sampleRate sample rate that you want to use.
     * @return The number of samples it would take (at the given sampleRate) to take duration
     *         in given timebase to play.
     */
    static int64_t timeDurationToSamples(int64_t duration, int sampleRate, const Rational timeBase = Rational(1, 1000000));


protected:
    // protected virtual
    virtual void setupDataPointers(const AVFrame *frame);

    void init(AVSampleFormat sampleFormat, int samplesCount, int channels, int samplesRate);
};

typedef std::shared_ptr<AudioSamples> AudioSamplesPtr;
typedef std::weak_ptr<AudioSamples> AudioSamplesWPtr;

} // namespace av

#endif // AV_AUDIOSAMPLES_H
