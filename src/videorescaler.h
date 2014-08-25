#ifndef VIDEORESCALER_H
#define VIDEORESCALER_H

#include <iostream>
#include <memory>

#include "ffmpeg.h"
#include "frame.h"
#include "videoframe.h"

namespace av
{

class VideoRescaler
{
public:
    VideoRescaler();

    VideoRescaler(int dstWidth, int dstHeight, AVPixelFormat dstPixelFormat,
                  int srcWidth, int srcHeight, AVPixelFormat srcPixelFormat);

    VideoRescaler(const VideoRescaler &other);
    VideoRescaler(VideoRescaler &&other);

    ~VideoRescaler();

    VideoRescaler& operator=(const VideoRescaler &rhs);
    VideoRescaler& operator=(VideoRescaler &&rhs);

    int getDstWidth() const { return dstWidth; }
    int getDstHeight() const { return dstHeight; }
    AVPixelFormat getDstPixelFormat() { return dstPixelFormat; }

    int getSrcWidth() const { return srcWidth; }
    int getSrcHeight() const { return srcHeight; }
    AVPixelFormat getSrcPixelFormat() { return srcPixelFormat; }

    int32_t resample(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame);

    bool isValid() const;

private:
    void swap(VideoRescaler &other) noexcept;

private:
    SwsContext   *context        = nullptr;

    int           dstWidth       = -1;
    int           dstHeight      = -1;
    AVPixelFormat dstPixelFormat = AV_PIX_FMT_NONE;

    int           srcWidth       = -1;
    int           srcHeight      = -1;
    AVPixelFormat srcPixelFormat = AV_PIX_FMT_NONE;

};

typedef std::shared_ptr<VideoRescaler> VideoResamplerPtr;
typedef std::weak_ptr<VideoRescaler> VideoResamplerWPtr;

} // ::av

#endif // VIDEORESAMPLER_H
