#ifndef VIDEORESCALER_H
#define VIDEORESCALER_H

#include <iostream>
#include <memory>

#include "ffmpeg.h"
#include "frame.h"
#include "avutils.h"

namespace av
{

class VideoRescaler : public FFWrapperPtr<SwsContext>, public noncopyable
{
public:
    VideoRescaler();

    VideoRescaler(int m_dstWidth, int m_dstHeight, AVPixelFormat m_dstPixelFormat,
                  int m_srcWidth, int m_srcHeight, AVPixelFormat m_srcPixelFormat) throw(std::invalid_argument);

    VideoRescaler(int m_dstWidth, int m_dstHeight, AVPixelFormat m_dstPixelFormat);

    VideoRescaler(const VideoRescaler &other);
    VideoRescaler(VideoRescaler &&other);

    ~VideoRescaler();

    VideoRescaler& operator=(const VideoRescaler &rhs);
    VideoRescaler& operator=(VideoRescaler &&rhs);

    int dstWidth() const { return m_dstWidth; }
    int dstHeight() const { return m_dstHeight; }
    AVPixelFormat dstPixelFormat() { return m_dstPixelFormat; }

    int srcWidth() const { return m_srcWidth; }
    int srcHeight() const { return m_srcHeight; }
    AVPixelFormat srcPixelFormat() { return m_srcPixelFormat; }

    int32_t rescale(VideoFrame2 &dst, const VideoFrame2 &src);

    bool isValid() const;

private:
    void swap(VideoRescaler &other) noexcept;

    void getContext();

private:
    int           m_dstWidth       = -1;
    int           m_dstHeight      = -1;
    AVPixelFormat m_dstPixelFormat = AV_PIX_FMT_NONE;

    int           m_srcWidth       = -1;
    int           m_srcHeight      = -1;
    AVPixelFormat m_srcPixelFormat = AV_PIX_FMT_NONE;

};

} // ::av

#endif // VIDEORESAMPLER_H
