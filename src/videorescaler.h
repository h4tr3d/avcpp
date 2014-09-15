#ifndef VIDEORESCALER_H
#define VIDEORESCALER_H

#include <iostream>
#include <memory>

#include "ffmpeg.h"
#include "frame.h"
#include "videoframe.h"

namespace av
{

class VideoRescaler : public FFWrapperPtr<SwsContext>
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

    attribute_deprecated2("Use dstWidth() instead")
    int getDstWidth() const { return m_dstWidth; }
    attribute_deprecated2("Use dstHeight() instead")
    int getDstHeight() const { return m_dstHeight; }
    attribute_deprecated2("Use dstPixelFormat() instead")
    AVPixelFormat getDstPixelFormat() { return m_dstPixelFormat; }

    int dstWidth() const { return m_dstWidth; }
    int dstHeight() const { return m_dstHeight; }
    AVPixelFormat dstPixelFormat() { return m_dstPixelFormat; }

    attribute_deprecated2("Use srcWidth() instead")
    int getSrcWidth() const { return m_srcWidth; }
    attribute_deprecated2("Use srcHeight() instead")
    int getSrcHeight() const { return m_srcHeight; }
    attribute_deprecated2("Use srcPixelFormat() instead")
    AVPixelFormat getSrcPixelFormat() { return m_srcPixelFormat; }

    int srcWidth() const { return m_srcWidth; }
    int srcHeight() const { return m_srcHeight; }
    AVPixelFormat srcPixelFormat() { return m_srcPixelFormat; }

    attribute_deprecated2("Use rescale(VideoFrame2 &dst, const VideoFrame2 &src) instead")
    int32_t rescale(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame);

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

typedef std::shared_ptr<VideoRescaler> VideoResamplerPtr;
typedef std::weak_ptr<VideoRescaler> VideoResamplerWPtr;

} // ::av

#endif // VIDEORESAMPLER_H
