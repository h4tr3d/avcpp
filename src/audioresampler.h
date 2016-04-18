#ifndef AV_AUDIORESAMPLER_H
#define AV_AUDIORESAMPLER_H

#include <functional>

#include "ffmpeg.h"
#include "frame.h"
#include "dictionary.h"
#include "avutils.h"
#include "sampleformat.h"
#include "averror.h"

namespace av {

class AudioResampler : public FFWrapperPtr<SwrContext>, public noncopyable
{
public:
    AudioResampler();

    AudioResampler(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   std::error_code &ec = throws());

    AudioResampler(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   Dictionary &options,
                   std::error_code &ec = throws());

    AudioResampler(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   Dictionary &&options,
                   std::error_code &ec = throws());

    ~AudioResampler();

    AudioResampler(AudioResampler &&other);
    AudioResampler& operator=(AudioResampler &&rhs);

    void swap(AudioResampler& other);

    int64_t        dstChannelLayout() const;
    int            dstChannels()      const;
    int            dstSampleRate()    const;
    SampleFormat   dstSampleFormat()  const;

    int64_t        srcChannelLayout() const;
    int            srcChannels()      const;
    int            srcSampleRate()    const;
    SampleFormat   srcSampleFormat()  const;

    /**
     * @brief Push frame to the rescaler context.
     * Note, signle push can produce multiple frames.
     * @param[in]     src    source frame
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     */
    void push(const AudioSamples2 &src, std::error_code &ec = throws());

    /**
     * @brief Pop frame from the rescaler context.
     *
     * Note, single push can produce multiple output frames.
     *
     * If @p getall sets to true and false returns, this means that no data at all and resampler
     * fully flushed.
     *
     * @param[out]    dst            frame to store audio data. Frame must be allocated before call.
     * @param[in]     getall         if true and if avail data less then nb_samples return data as is
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return false if no samples avail, true otherwise. On error false.
     */
    bool pop(AudioSamples2 &dst, bool getall, std::error_code &ec = throws());

    /**
     * @brief Pop frame from the rescaler context.
     *
     * Note, single push can produce multiple output frames.
     *
     * If @p samplesCount sets to the zero and null frame returns, it means that no data at all and
     * resampler fully flushed.
     *
     * @param[in]     samplesCount  samples count to extract. If zero (0) - extract all delayed (buffered) samples.
     * @param[in,out] ec            this represents the error status on exit, if this is pre-initialized to
     *                              av#throws the function will throw on error instead
     * @return resampled samples or null-frame when no requested samples avail. On error null-frame.
     */
    AudioSamples2 pop(size_t samplesCount, std::error_code &ec = throws());

    bool isValid() const;
    operator bool() const { return isValid(); }

    int64_t delay() const;

    bool init(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              std::error_code &ec = throws());

    bool init(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              Dictionary &options,
              std::error_code &ec = throws());

    bool init(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              Dictionary &&options,
              std::error_code &ec = throws());

    static
    bool validate(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat);

private:
    bool init(int64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              int64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              AVDictionary **dict, std::error_code &ec);

private:
    // Cached values to avoid access to the av_opt
    int64_t        m_dstChannelsLayout;
    int            m_dstRate;
    SampleFormat   m_dstFormat;
    int64_t        m_srcChannelsLayout;
    int            m_srcRate;
    SampleFormat   m_srcFormat;

    int            m_streamIndex = -1;
    Timestamp      m_prevPts;
    Timestamp      m_nextPts;
};

} // namespace av

#endif // AV_AUDIORESAMPLER_H
