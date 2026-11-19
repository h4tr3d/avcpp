#pragma once

/**
 * Video scaling and pixel format conversion with libswscale.
 *
 * @file
 * @ingroup frame
 */

#include <iostream>
#include <memory>

#include "ffmpeg.h"
#include "frame.h"
#include "avutils.h"
#include "pixelformat.h"
#include "averror.h"

namespace av
{

/**
 * Scaling algorithms and options of libswscale.
 *
 * The values mirror `SWS_*`; the avcpp names only prefix them for readability.
 */
enum SwsFlags
{
    /// Let libswscale pick the algorithm.
    SwsFlagAuto = 0,
    /// Bilinear filtering with reduced chroma interpolation.
    SwsFlagFastBilinear = SWS_FAST_BILINEAR,
    /// Bilinear filtering.
    SwsFlagBilinear = SWS_BILINEAR,
    /// Bicubic filtering.
    SwsFlagBicubic = SWS_BICUBIC,
    /// Experimental filter, kept for compatibility.
    SwsFlagExprerimental = SWS_X,
    /// Nearest neighbour, the fastest and crudest option.
    SwsFlagNeighbor = SWS_POINT,
    /// Averaging over the source area; good for downscaling.
    SwsFlagArea = SWS_AREA,
    /// Bicubic for luma, bilinear for chroma.
    SwsFlagBicublin = SWS_BICUBLIN,
    /// Gaussian filter.
    SwsFlagGauss = SWS_GAUSS,
    /// Sinc filter.
    SwsFlagSinc = SWS_SINC,
    /// Lanczos filter, a good default for high quality.
    SwsFlagLanczos = SWS_LANCZOS,
    /// Spline filter.
    SwsFlagSpline = SWS_SPLINE,
    /// Print the chosen filter configuration.
    SwsFlagPrintInfo = SWS_PRINT_INFO,
    /// Rounding mode that costs speed and gains accuracy.
    SwsFlagAccurateRnd = SWS_ACCURATE_RND,
    /// Full chroma interpolation by the scaler.
    SwsFlagFullChromaInt = SWS_FULL_CHR_H_INT,
    /// Treat the input chroma as full resolution.
    SwsFlagFullChromaInp = SWS_FULL_CHR_H_INP,
    /// Bit-exact output where the algorithm allows it.
    SwsFlagBitexact = SWS_BITEXACT,
    /// Error diffusion for palette conversion.
    SwsFlagErrorDiffusion = SWS_ERROR_DIFFUSION,
};

/**
 * Converts video frames between sizes and pixel formats.
 *
 * The scaler works on system memory only: a hardware frame must be transferred
 * with VideoFrame::toSystemMemory() first and uploaded back afterwards. Both
 * the source and the destination geometry are fixed at construction, because
 * libswscale allocates its tables for one conversion.
 */
class VideoRescaler : public FFWrapperPtr<SwsContext>, public noncopyable
{
public:
    /**
     * Create an uninitialised scaler.
     *
     * Configure it with rescale() by passing frames of the target geometry, or
     * construct it with explicit sizes.
     */
    VideoRescaler();

    /**
     * Create a scaler with explicit source and destination.
     *
     * @param dstWidth       output width in pixels
     * @param dstHeight      output height in pixels
     * @param dstPixelFormat output pixel format
     * @param srcWidth       input width in pixels
     * @param srcHeight      input height in pixels
     * @param srcPixelFormat input pixel format
     * @param flags          a combination of SwsFlags
     */
    VideoRescaler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat,
                  int srcWidth, int srcHeight, PixelFormat srcPixelFormat,
                  int32_t flags = SwsFlagAuto);

    /**
     * Create a scaler that keeps the input geometry and only converts formats.
     *
     * @param m_dstWidth       output width in pixels
     * @param m_dstHeight      output height in pixels
     * @param m_dstPixelFormat output pixel format
     * @param flags            a combination of SwsFlags
     */
    VideoRescaler(int m_dstWidth, int m_dstHeight, PixelFormat m_dstPixelFormat, int32_t flags = SwsFlagAuto);

    /**
     * Copy the scaler configuration.
     *
     * @param other the scaler to copy
     */
    VideoRescaler(const VideoRescaler &other);
    /**
     * Move the scaler.
     *
     * @param other the scaler to move from; left uninitialised
     */
    VideoRescaler(VideoRescaler &&other);

    ~VideoRescaler();

    /**
     * Copy assignment: replaces the configuration and the libswscale context.
     *
     * @param rhs the scaler to copy from
     *
     * @return this scaler
     */
    VideoRescaler& operator=(const VideoRescaler &rhs);
    /**
     * Move assignment.
     *
     * @param rhs the scaler to move from; left uninitialised
     *
     * @return this scaler
     */
    VideoRescaler& operator=(VideoRescaler &&rhs);

    /**
     * Output width.
     *
     * @return the width in pixels, or -1 when it is not configured
     */
    int dstWidth() const { return m_dstWidth; }
    /**
     * Output height.
     *
     * @return the height in pixels, or -1 when it is not configured
     */
    int dstHeight() const { return m_dstHeight; }
    /**
     * Output pixel format.
     *
     * @return the format, or `AV_PIX_FMT_NONE` when it is not configured
     */
    PixelFormat dstPixelFormat() { return m_dstPixelFormat; }

    /**
     * Input width.
     *
     * @return the width in pixels, or -1 when it is not configured
     */
    int srcWidth() const { return m_srcWidth; }
    /**
     * Input height.
     *
     * @return the height in pixels, or -1 when it is not configured
     */
    int srcHeight() const { return m_srcHeight; }
    /**
     * Input pixel format.
     *
     * @return the format, or `AV_PIX_FMT_NONE` when it is not configured
     */
    PixelFormat srcPixelFormat() { return m_srcPixelFormat; }

    /**
     * Scaling options in use.
     *
     * @return a combination of SwsFlags
     */
    int32_t flags() const { return m_flags; }

    /**
     * Scale into an existing frame.
     *
     * @param dst destination frame; it must be allocated with the output
     *            geometry and format
     * @param src source frame in system memory
     * @param ec  holder for the error code
     */
    void       rescale(VideoFrame &dst, const VideoFrame &src, OptionalErrorCode ec = throws());
    /**
     * Scale into a newly allocated frame.
     *
     * @param src source frame in system memory
     * @param ec  holder for the error code
     *
     * @return the scaled frame, or an invalid frame on failure
     */
    VideoFrame rescale(const VideoFrame &src, OptionalErrorCode ec = throws());

    /**
     * Whether the scaler has a libswscale context.
     *
     * @return `true` when rescale() can be called
     */
    bool isValid() const;

private:
    void swap(VideoRescaler &other) noexcept;

    void getContext(int32_t flags = 0);

    static
    bool validate(int width, int height, PixelFormat pixelFormat);

private:
    int           m_dstWidth       = -1;
    int           m_dstHeight      = -1;
    PixelFormat   m_dstPixelFormat = AV_PIX_FMT_NONE;

    int           m_srcWidth       = -1;
    int           m_srcHeight      = -1;
    PixelFormat   m_srcPixelFormat = AV_PIX_FMT_NONE;

    int32_t       m_flags          = SwsFlagAuto;
};

} // ::av

