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
        Default = 0,
        None    = 1
    };

    using Parent = PixSampleFmtWrapper<SampleFormat, AVSampleFormat>;
    using Parent::PixSampleFmtWrapper;

    explicit SampleFormat(const char* name) noexcept;
    explicit SampleFormat(const std::string& name) noexcept;

    const char* name(std::error_code &ec = throws()) const;

    SampleFormat alternativeSampleFormat(bool isPlanar) const noexcept;
    SampleFormat packedSampleFormat() const noexcept;
    SampleFormat planarSampleFormat() const noexcept;

    bool isPlanar() const noexcept;

    size_t bytesPerSample(std::error_code& ec = throws()) const;

    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, std::error_code& ec = throws()) const;
    size_t requiredBufferSize(int nbChannels, int nbSamples, int align, int &lineSize, std::error_code& ec = throws()) const;
};

} // namespace av

