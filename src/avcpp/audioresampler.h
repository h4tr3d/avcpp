#pragma once

/**
 * Audio resampling with libswresample.
 *
 * @file
 * @ingroup frame
 */

#include <functional>

#include "ffmpeg.h"
#include "frame.h"
#include "dictionary.h"
#include "avutils.h"
#include "sampleformat.h"
#include "averror.h"

namespace av {

// TODO: support New Channel Layout directly

/**
 * Converts audio between sample rates, sample formats and channel layouts.
 *
 * The typical use is a decode-transform-encode pipeline: the decoder produces
 * whatever the stream contains, the encoder accepts one specific layout, and
 * this is the component in between. Construct it with both layouts, or call
 * init() later; push() feeds samples in and pop() takes them out.
 *
 * A single push can produce several output buffers, and the resampler holds an
 * internal delay, so popping until it returns nothing is part of the contract.
 */
class AudioResampler : public FFWrapperPtr<SwrContext>, public noncopyable
{
public:
    /**
     * Create an uninitialised resampler.
     *
     * Nothing can be pushed until init() succeeds.
     */
    AudioResampler();

    /**
     * Create and initialise a resampler.
     *
     * @param dstChannelsLayout channel layout to produce, as an `AV_CH_LAYOUT_*` mask
     * @param dstRate           sample rate to produce, in hertz
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input, in hertz
     * @param srcFormat         sample format of the input
     * @param ec                holder for the error code
     */
    AudioResampler(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   OptionalErrorCode ec = throws());

    /**
     * Create and initialise a resampler with options.
     *
     * @param dstChannelsLayout channel layout to produce
     * @param dstRate           sample rate to produce
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input
     * @param srcFormat         sample format of the input
     * @param options           resampler options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec                holder for the error code
     */
    AudioResampler(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   Dictionary &options,
                   OptionalErrorCode ec = throws());

    /**
     * Create and initialise a resampler with options, moving the dictionary.
     *
     * @param dstChannelsLayout channel layout to produce
     * @param dstRate           sample rate to produce
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input
     * @param srcFormat         sample format of the input
     * @param options           resampler options, moved into the call
     * @param ec                holder for the error code
     */
    AudioResampler(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
                   uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
                   Dictionary &&options,
                   OptionalErrorCode ec = throws());

    ~AudioResampler();

    /**
     * Move a resampler.
     *
     * @param other the resampler to move from; left uninitialised
     */
    AudioResampler(AudioResampler &&other);
    /**
     * Move assignment.
     *
     * @param rhs the resampler to move from; left uninitialised
     *
     * @return this resampler
     */
    AudioResampler& operator=(AudioResampler &&rhs);

    /**
     * Exchange the state of two resamplers.
     *
     * @param other the resampler to swap with
     */
    void swap(AudioResampler& other);

    /**
     * Channel layout the resampler produces.
     *
     * @return an `AV_CH_LAYOUT_*` mask
     */
    uint64_t       dstChannelLayout() const;
    /**
     * Number of channels the resampler produces.
     *
     * @return the channel count
     */
    int            dstChannels()      const;
    /**
     * Sample rate the resampler produces.
     *
     * @return samples per second
     */
    int            dstSampleRate()    const;
    /**
     * Sample format the resampler produces.
     *
     * @return the format
     */
    SampleFormat   dstSampleFormat()  const;

    /**
     * Channel layout the resampler expects.
     *
     * @return an `AV_CH_LAYOUT_*` mask
     */
    uint64_t       srcChannelLayout() const;
    /**
     * Number of channels the resampler expects.
     *
     * @return the channel count
     */
    int            srcChannels()      const;
    /**
     * Sample rate the resampler expects.
     *
     * @return samples per second
     */
    int            srcSampleRate()    const;
    /**
     * Sample format the resampler expects.
     *
     * @return the format
     */
    SampleFormat   srcSampleFormat()  const;

    /**
     * @brief Push frame to the rescaler context.
     * Note, signle push can produce multiple frames.
     * @param[in]     src    source frame
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     */
    void push(const AudioSamples &src, OptionalErrorCode ec = throws());

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
    bool pop(AudioSamples &dst, bool getall, OptionalErrorCode ec = throws());

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
    AudioSamples pop(size_t samplesCount, OptionalErrorCode ec = throws());

    /**
     * Whether the resampler is initialised.
     *
     * @return `true` after a successful init()
     */
    bool isValid() const;
    /**
     * Whether the resampler is initialised.
     *
     * @return `true` after a successful init()
     */
    operator bool() const { return isValid(); }

    /**
     * Samples still buffered inside the resampler.
     *
     * @return the delay in output samples
     */
    int64_t delay() const;

    /**
     * Initialise the resampler.
     *
     * @param dstChannelsLayout channel layout to produce, as an `AV_CH_LAYOUT_*` mask
     * @param dstRate           sample rate to produce, in hertz
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input, in hertz
     * @param srcFormat         sample format of the input
     * @param ec                holder for the error code
     *
     * @return `true` when the resampler is ready
     */
    bool init(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              OptionalErrorCode ec = throws());

    /**
     * Initialise the resampler with options.
     *
     * @param dstChannelsLayout channel layout to produce
     * @param dstRate           sample rate to produce
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input
     * @param srcFormat         sample format of the input
     * @param options           resampler options; the object stays usable, but FFmpeg removes the options it recognized
     * @param ec                holder for the error code
     *
     * @return `true` when the resampler is ready
     */
    bool init(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              Dictionary &options,
              OptionalErrorCode ec = throws());

    /**
     * Initialise the resampler with options, moving the dictionary.
     *
     * @param dstChannelsLayout channel layout to produce
     * @param dstRate           sample rate to produce
     * @param dstFormat         sample format to produce
     * @param srcChannelsLayout channel layout of the input
     * @param srcRate           sample rate of the input
     * @param srcFormat         sample format of the input
     * @param options           resampler options, moved into the call
     * @param ec                holder for the error code
     *
     * @return `true` when the resampler is ready
     */
    bool init(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              Dictionary &&options,
              OptionalErrorCode ec = throws());

    static
    /**
     * Whether the output parameters are usable before the input is known.
     *
     * @param dstChannelsLayout channel layout to produce
     * @param dstRate           sample rate to produce
     * @param dstFormat         sample format to produce
     *
     * @return `true` when the resampler initialises and accepts these values
     */
    bool validate(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat);

private:
    bool init(uint64_t dstChannelsLayout, int dstRate, SampleFormat dstFormat,
              uint64_t srcChannelsLayout, int srcRate, SampleFormat srcFormat,
              AVDictionary **dict, OptionalErrorCode ec);

private:
    // Cached values to avoid access to the av_opt
    uint64_t       m_dstChannelsLayout;
    int            m_dstRate;
    SampleFormat   m_dstFormat;
    uint64_t       m_srcChannelsLayout;
    int            m_srcRate;
    SampleFormat   m_srcFormat;

    int            m_streamIndex = -1;
    Timestamp      m_prevPts;
    Timestamp      m_nextPts;

    int64_t        m_filterSize;
};

} // namespace av

