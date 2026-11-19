#pragma once

/**
 * Audio sample formats.
 *
 * @file
 * @ingroup frame
 */

#include <iostream>
#include <string>

#include "averror.h"
#include "ffmpeg.h"

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace av {

/**
 * @brief The SampleFormat class is a simple proxy class for AVSampleFormat
 *
 * It allows to quckly ask some info about sample format. Only base one is provided.
 *
 */
/**
 * Audio sample format descriptor.
 *
 * Describes how samples of one channel are stored: the numeric type, the
 * interleaving, and therefore how much memory a buffer needs.
 */
class SampleFormat : public PixSampleFmtWrapper<SampleFormat, AVSampleFormat>
{
public:
    /**
     * Alignment expected from a sample buffer.
     *
     * The values mirror the `align` argument of the `av_samples_*` helpers.
     */
    enum Alignment
    {
        /// Align each line to the interval FFmpeg prefers.
        AlignDefault = 0,
        /// Do not align: lines follow each other directly.
        AlignNone    = 1
    };

    /// The wrapper this type builds on.
    using Parent = PixSampleFmtWrapper<SampleFormat, AVSampleFormat>;
    using Parent::Parent;

    /**
     * Create an unset format, equivalent to `AV_SAMPLE_FMT_NONE`.
     */
    SampleFormat() = default;
    /**
     * Look a sample format up by name.
     *
     * @param name the name FFmpeg uses, for example "s16"
     */
    explicit SampleFormat(const char* name) noexcept;
    /**
     * Look a sample format up by name.
     *
     * @param name the name FFmpeg uses, for example "s16"
     */
    explicit SampleFormat(const std::string& name) noexcept;

    /**
     * Name of the format.
     *
     * @param ec holder for the error code; an unset format reports an error
     *
     * @return a static string owned by FFmpeg
     */
    const char* name(OptionalErrorCode ec = throws()) const;

    /**
     * The same numeric type in the requested layout.
     *
     * @param isPlanar `true` for a planar format, `false` for a packed one
     *
     * @return the matching format, or an unset one when there is no such pair
     */
    SampleFormat alternativeSampleFormat(bool isPlanar) const noexcept;
    /**
     * The packed variant of this format.
     *
     * @return the packed format, or the same value when it already is packed
     */
    SampleFormat packedSampleFormat() const noexcept;
    /**
     * The planar variant of this format.
     *
     * @return the planar format, or the same value when it already is planar
     */
    SampleFormat planarSampleFormat() const noexcept;

    /**
     * Whether the channels are stored in separate planes.
     *
     * @return `true` for planar formats
     */
    bool isPlanar() const noexcept;

    /**
     * Bytes needed for one sample of one channel.
     *
     * @param ec holder for the error code
     *
     * @return the size in bytes
     */
    size_t bytesPerSample(OptionalErrorCode ec = throws()) const;
    /**
     * Bits per sample of one channel.
     *
     * Unlike bytesPerSample(), this reports the stored depth of the type, so a
     * 24-bit sample in a 32-bit container reports 24.
     *
     * @param ec holder for the error code
     *
     * @return the bit depth
     */
    size_t bitsPerSample(OptionalErrorCode ec = throws()) const;

    /**
     * Size of a buffer for the given amount of audio.
     *
     * @param nbChannels number of channels
     * @param nbSamples  number of samples per channel
     * @param align      an Alignment value
     * @param ec         holder for the error code
     *
     * @return the required size in bytes
     */
    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, OptionalErrorCode ec = throws()) const;
    /**
     * Size of a buffer for the given amount of audio, and its line size.
     *
     * @param nbChannels number of channels
     * @param nbSamples  number of samples per channel
     * @param align      an Alignment value
     * @param lineSize   receives the size of one line in bytes
     * @param ec         holder for the error code
     *
     * @return the required size in bytes
     */
    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, int &lineSize, OptionalErrorCode ec = throws()) const;

    // Static helper methods

    /**
     * Size of a buffer for the given amount of audio.
     *
     * @param fmt        sample format
     * @param nbChannels number of channels
     * @param nbSamples  number of samples per channel
     * @param align      an Alignment value
     * @param ec         holder for the error code
     *
     * @return the required size in bytes
     */
    static size_t requiredBufferSize(SampleFormat fmt, int nbChannels, int nbSamples, int align, OptionalErrorCode ec = throws());
    /**
     * Size of a buffer for the given amount of audio, and its line size.
     *
     * @param fmt        sample format
     * @param nbChannels number of channels
     * @param nbSamples  number of samples per channel
     * @param align      an Alignment value
     * @param lineSize   receives the size of one line in bytes
     * @param ec         holder for the error code
     *
     * @return the required size in bytes
     */
    static size_t requiredBufferSize(SampleFormat fmt, int nbChannels, int nbSamples, int align, int &lineSize, OptionalErrorCode ec = throws());

    /**
     * Point the per-channel data pointers at a contiguous buffer.
     *
     * @param audioData  array that receives one pointer per plane
     * @param linesize   array that receives the line size per plane
     * @param buf        the buffer to split
     * @param nbChannels number of channels
     * @param nbSamples  number of samples per channel
     * @param fmt        sample format
     * @param align      an Alignment value
     * @param ec         holder for the error code
     */
    static void fillArrays(uint8_t **audioData,
                           int *linesize,
                           const uint8_t *buf,
                           int nbChannels,
                           int nbSamples,
                           SampleFormat fmt,
                           int align,
                           OptionalErrorCode ec = throws());

    /**
     * Write silence into a buffer.
     *
     * The silent value depends on the format: zero for unsigned samples and for
     * floating point, the middle of the range for signed ones.
     *
     * @param audioData  per-plane data pointers
     * @param offset     offset in bytes to start at
     * @param nbSamples  number of samples per channel to silence
     * @param nbChannels number of channels
     * @param fmt        sample format
     */
    static void setSilence(uint8_t **audioData,
                           int offset,
                           int nbSamples,
                           int nbChannels,
                           SampleFormat fmt);

#if 0
    // WARNING: use this function only if required API call expect data allocated with av_alloc() proc families.
    // You must free allocated data with av_free(&audioData[0]);
    static void samplesAlloc(uint8_t **audioData,
                             int *linesize,
                             int nbChannels,
                             int nbSamples,
                             SampleFormat fmt,
                             int align,
                             OptionalErrorCode ec = throws());
#endif

};

} // namespace av

