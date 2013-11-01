#ifndef VIDEORESAMPLER_H
#define VIDEORESAMPLER_H

#include <iostream>

#include <boost/smart_ptr.hpp>

#include "ffmpeg.h"
#include "frame.h"
#include "videoframe.h"

namespace av
{

using namespace std;

class VideoResampler;
typedef boost::shared_ptr<VideoResampler> VideoResamplerPtr;
typedef boost::weak_ptr<VideoResampler> VideoResamplerWPtr;


class VideoResampler
{
public:
    VideoResampler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat,
                   int srcWidth, int srcHeight, PixelFormat srcPixelFormat);
    ~VideoResampler();


    int getDstWidth() const { return dstWidth; }
    int getDstHieght() const { return dstHeight; }
    PixelFormat getDstPixelFormat() { return dstPixelFormat; }

    int getSrcWidth() const { return srcWidth; }
    int getSrcHeight() const { return srcHeight; }
    PixelFormat getSrcPixelFormat() { return srcPixelFormat; }

    int32_t resample(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame);

    bool isValid() const;

private:
    SwsContext *context;

    int         dstWidth;
    int         dstHeight;
    PixelFormat dstPixelFormat;

    int         srcWidth;
    int         srcHeight;
    PixelFormat srcPixelFormat;

};

} // ::av

#endif // VIDEORESAMPLER_H
