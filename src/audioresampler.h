#ifndef AV_AUDIORESAMPLER_H
#define AV_AUDIORESAMPLER_H

#include "ffmpeg.h"
#include "audiosamples.h"

namespace av {

class AudioResampler
{
public:
    AudioResampler();
    AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                   int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat);
    ~AudioResampler();

    int64_t        getDstChannelLayout() const { return dstChannelsLayout; }
    int            getDstChannels()      const { return av_get_channel_layout_nb_channels(dstChannelsLayout); }
    int            getDstSampleRate()    const { return dstRate; }
    AVSampleFormat getDstSampleFormat()  const { return dstFormat; }

    int64_t        getSrcChannelLayout() const { return srcChannelsLayout; }
    int            getSrcChannels()      const { return av_get_channel_layout_nb_channels(srcChannelsLayout); }
    int            getSrcSampleRate()    const { return srcRate; }
    AVSampleFormat getSrcSampleFormat()  const { return srcFormat; }

    int32_t        resample(const AudioSamplesPtr& dst, const AudioSamplesPtr& src, uint samplesCount);

    bool           isValid() const;

    bool init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
              int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat);

private:
    SwrContext    *context;

    int64_t        dstChannelsLayout;
    int            dstRate;
    AVSampleFormat dstFormat;

    int64_t        srcChannelsLayout;
    int            srcRate;
    AVSampleFormat srcFormat;
};

typedef boost::shared_ptr<AudioResampler> AudioResamplerPtr;
typedef boost::weak_ptr<AudioResampler>   AudioResamplerWPtr;

} // namespace av

#endif // AV_AUDIORESAMPLER_H
