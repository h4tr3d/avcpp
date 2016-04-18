#pragma once

#include <string>
#include <iostream>

#include "averror.h"
#include "ffmpeg.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}

// We need not to be back compatible:
#ifdef PixelFormat
#undef PixelFormat
#endif

namespace av {

/**
 * @brief The PixelFormat class is a simple wrapper for AVPixelFormat that allow to acces it it properties
 *
 * It has size same to the AVPixelFormat, but wrap pixel description function too.
 */
class PixelFormat : public PixSampleFmtWrapper<PixelFormat, AVPixelFormat>
{
public:
    using Parent = PixSampleFmtWrapper<PixelFormat, AVPixelFormat>;
    using Parent::Parent;

    PixelFormat() = default;
    explicit PixelFormat(const char* name) noexcept;
    explicit PixelFormat(const std::string& name) noexcept;

    // Info functions
    const char* name(std::error_code &ec = throws()) const;

    const AVPixFmtDescriptor* descriptor(std::error_code &ec = throws()) const;

    int bitsPerPixel(std::error_code &ec = throws()) const;

    size_t planesCount(std::error_code &ec = throws()) const;

    PixelFormat swapEndianness() const noexcept;

    // See FF_LOSS_xxx flags
    size_t convertionLoss(PixelFormat dstFmt, bool srcHasAlpha = false) const noexcept;
};

} // namespace av

