#pragma once

/**
 * Pixel formats.
 *
 * @file
 * @ingroup frame
 */

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
/**
 * Pixel format descriptor.
 *
 * A thin wrapper over `AVPixelFormat` that can answer the questions a pipeline
 * asks: the name of the format, how many planes it has, how many bits per pixel
 * it needs, and what a conversion to another format would cost.
 */
class PixelFormat : public PixSampleFmtWrapper<PixelFormat, AVPixelFormat>
{
public:
    /// The wrapper this type builds on.
    using Parent = PixSampleFmtWrapper<PixelFormat, AVPixelFormat>;
    using Parent::Parent;

    /**
     * Create an unset format, equivalent to `AV_PIX_FMT_NONE`.
     */
    PixelFormat() = default;
    /**
     * Look a pixel format up by name.
     *
     * @param name the name FFmpeg uses, for example "yuv420p"
     */
    explicit PixelFormat(const char* name) noexcept;
    /**
     * Look a pixel format up by name.
     *
     * @param name the name FFmpeg uses, for example "yuv420p"
     */
    explicit PixelFormat(const std::string& name) noexcept;

    // Info functions
    /**
     * Name of the format.
     *
     * @param ec holder for the error code; an unset format reports an error
     *
     * @return a static string owned by FFmpeg
     */
    const char* name(OptionalErrorCode ec = throws()) const;

    /**
     * FFmpeg description of the format.
     *
     * @param ec holder for the error code
     *
     * @return the descriptor, which describes planes, chroma subsampling and
     *         bit depth
     */
    const AVPixFmtDescriptor* descriptor(OptionalErrorCode ec = throws()) const;

    /**
     * Bits needed for one pixel.
     *
     * @param ec holder for the error code
     *
     * @return the average bit depth, accounting for chroma subsampling
     */
    int bitsPerPixel(OptionalErrorCode ec = throws()) const;

    /**
     * Number of image planes.
     *
     * @param ec holder for the error code
     *
     * @return 1 for packed formats, 2 to 4 for planar ones
     */
    size_t planesCount(OptionalErrorCode ec = throws()) const;

    /**
     * The same format with the opposite byte order.
     *
     * @return the matching `*BE` or `*LE` format, or the same value when the
     *         format has no endianness variant
     */
    PixelFormat swapEndianness() const noexcept;

    // See FF_LOSS_xxx flags
    /**
     * What a conversion to another format would lose.
     *
     * @param dstFmt      target format
     * @param srcHasAlpha whether the source actually carries alpha
     *
     * @return a combination of the `FF_LOSS_*` flags, zero when the conversion
     *         is lossless
     */
    size_t convertionLoss(PixelFormat dstFmt, bool srcHasAlpha = false) const noexcept;
};

} // namespace av

