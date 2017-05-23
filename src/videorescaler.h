#ifndef VIDEORESCALER_H
#define VIDEORESCALER_H

#include <iostream>
#include <memory>

#include "ffmpeg.h"
#include "frame.h"
#include "avutils.h"
#include "pixelformat.h"
#include "averror.h"

namespace av
{

enum SwsFlags
{
    SwsFlagAuto = 0,
    SwsFlagFastBilinear = SWS_FAST_BILINEAR,
    SwsFlagBilinear = SWS_BILINEAR,
    SwsFlagBicubic = SWS_BICUBIC,
    SwsFlagExprerimental = SWS_X,
    SwsFlagNeighbor = SWS_POINT,
    SwsFlagArea = SWS_AREA,
    SwsFlagBicublin = SWS_BICUBLIN,
    SwsFlagGauss = SWS_GAUSS,
    SwsFlagSinc = SWS_SINC,
    SwsFlagLanczos = SWS_LANCZOS,
    SwsFlagSpline = SWS_SPLINE,
    SwsFlagPrintInfo = SWS_PRINT_INFO,
    SwsFlagAccurateRnd = SWS_ACCURATE_RND,
    SwsFlagFullChromaInt = SWS_FULL_CHR_H_INT,
    SwsFlagFullChromaInp = SWS_FULL_CHR_H_INP,
    SwsFlagBitexact = SWS_BITEXACT,
    SwsFlagErrorDiffusion = SWS_ERROR_DIFFUSION,
};

class VideoRescaler : public FFWrapperPtr<SwsContext>, public noncopyable
{
public:
    VideoRescaler();

    VideoRescaler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat,
                  int srcWidth, int srcHeight, PixelFormat srcPixelFormat,
                  int32_t flags = SwsFlagAuto);

    VideoRescaler(int m_dstWidth, int m_dstHeight, PixelFormat m_dstPixelFormat, int32_t flags = SwsFlagAuto);

    VideoRescaler(const VideoRescaler &other);
    VideoRescaler(VideoRescaler &&other);

    ~VideoRescaler();

    VideoRescaler& operator=(const VideoRescaler &rhs);
    VideoRescaler& operator=(VideoRescaler &&rhs);

    int dstWidth() const { return m_dstWidth; }
    int dstHeight() const { return m_dstHeight; }
    PixelFormat dstPixelFormat() { return m_dstPixelFormat; }

    int srcWidth() const { return m_srcWidth; }
    int srcHeight() const { return m_srcHeight; }
    PixelFormat srcPixelFormat() { return m_srcPixelFormat; }

    int32_t flags() const { return m_flags; }

    void        rescale(VideoFrame &dst, const VideoFrame &src, OptionalErrorCode ec = throws());
    VideoFrame rescale(const VideoFrame &src, OptionalErrorCode ec);

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

#endif // VIDEORESAMPLER_H
