#ifndef AV_AUDIORESAMPLER_H
#define AV_AUDIORESAMPLER_H

#include <functional>

#include "ffmpeg.h"
#include "frame.h"
#include "avutils.h"

namespace av {

class AudioResampler : public FFWrapperPtr<SwrContext>, public noncopyable
{
public:
    AudioResampler();
    AudioResampler(int64_t m_dstChannelsLayout, int m_dstRate, AVSampleFormat m_dstFormat,
                   int64_t m_srcChannelsLayout, int m_srcRate, AVSampleFormat m_srcFormat);
    ~AudioResampler();

    AudioResampler(AudioResampler &&other);
    AudioResampler& operator=(AudioResampler &&rhs);

    void swap(AudioResampler& other);

    int64_t        dstChannelLayout() const { return m_dstChannelsLayout; }
    int            dstChannels()      const { return av_get_channel_layout_nb_channels(m_dstChannelsLayout); }
    int            dstSampleRate()    const { return m_dstRate; }
    AVSampleFormat dstSampleFormat()  const { return m_dstFormat; }

    int64_t        srcChannelLayout() const { return m_srcChannelsLayout; }
    int            srcChannels()      const { return av_get_channel_layout_nb_channels(m_srcChannelsLayout); }
    int            srcSampleRate()    const { return m_srcRate; }
    AVSampleFormat srcSampleFormat()  const { return m_srcFormat; }

    /**
     * Push frame to the rescaler context. Note, signle push can produce multiple frames.
     * @param src source frame
     * @return 0 on success, error code otherwise
     */
    int32_t        push(const AudioSamples2 &src);

    /**
     * Pop frame from the rescaler context. Note, single push can produce multiple output frames.
     * @param dst          frame to store audio data
     * @param getAllAvail  if true and if avail data less then nb_samples return data as is
     * @return 0 if no frame avail, less 0 - error code, otherwise - samples count per channel
     */
    int32_t        pop(AudioSamples2 &dst, bool getAllAvail = false);

    bool           isValid() const;

    bool init(int64_t m_dstChannelsLayout, int m_dstRate, AVSampleFormat m_dstFormat,
              int64_t m_srcChannelsLayout, int m_srcRate, AVSampleFormat m_srcFormat);

private:
    int64_t        m_dstChannelsLayout;
    int            m_dstRate;
    AVSampleFormat m_dstFormat;

    int64_t        m_srcChannelsLayout;
    int            m_srcRate;
    AVSampleFormat m_srcFormat;

    int            m_streamIndex = -1;
    int64_t        m_prevPts     = AV_NOPTS_VALUE;
    int64_t        m_nextPts     = AV_NOPTS_VALUE;
};

} // namespace av

#endif // AV_AUDIORESAMPLER_H
