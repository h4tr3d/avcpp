#pragma once

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
class SampleFormat : public PixSampleFmtWrapper<SampleFormat, AVSampleFormat>
{
public:
    enum Alignment
    {
        AlignDefault = 0,
        AlignNone    = 1
    };

    using Parent = PixSampleFmtWrapper<SampleFormat, AVSampleFormat>;
    using Parent::Parent;

    SampleFormat() = default;
    explicit SampleFormat(const char* name) noexcept;
    explicit SampleFormat(const std::string& name) noexcept;

    const char* name(OptionalErrorCode ec = throws()) const;

    SampleFormat alternativeSampleFormat(bool isPlanar) const noexcept;
    SampleFormat packedSampleFormat() const noexcept;
    SampleFormat planarSampleFormat() const noexcept;

    bool isPlanar() const noexcept;

    size_t bytesPerSample(OptionalErrorCode ec = throws()) const;
    size_t bitsPerSample(OptionalErrorCode ec = throws()) const;

    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, OptionalErrorCode ec = throws()) const;
    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, int &lineSize, OptionalErrorCode ec = throws()) const;

    // Static helper methods

    static size_t requiredBufferSize(SampleFormat fmt, int nbChannels, int nbSamples, int align, OptionalErrorCode ec = throws());
    static size_t requiredBufferSize(SampleFormat fmt, int nbChannels, int nbSamples, int align, int &lineSize, OptionalErrorCode ec = throws());

    static void fillArrays(uint8_t **audioData,
                           int *linesize,
                           const uint8_t *buf,
                           int nbChannels,
                           int nbSamples,
                           SampleFormat fmt,
                           int align,
                           OptionalErrorCode ec = throws());

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

