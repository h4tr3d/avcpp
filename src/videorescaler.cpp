#include "videorescaler.h"

using namespace std;

namespace av
{

VideoRescaler::VideoRescaler()
{}

VideoRescaler::VideoRescaler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat, int srcWidth, int srcHeight, PixelFormat srcPixelFormat)
    : dstWidth(dstWidth),
      dstHeight(dstHeight),
      dstPixelFormat(dstPixelFormat),
      srcWidth(srcWidth),
      srcHeight(srcHeight),
      srcPixelFormat(srcPixelFormat)
{
    int32_t flags = 0;

    if (srcWidth < dstWidth)
        flags = SWS_BICUBIC;
    else
        flags = SWS_AREA;

    context = 0;
    if (srcPixelFormat != PIX_FMT_NONE)
    {
        context = sws_getContext(srcWidth, srcHeight, srcPixelFormat,
                                 dstWidth, dstHeight, dstPixelFormat,
                                 flags,
                                 0, 0, 0);
    }
}

VideoRescaler::VideoRescaler(const VideoRescaler &other)
    : VideoRescaler(other.dstWidth, other.dstHeight, other.dstPixelFormat,
                    other.srcWidth, other.srcHeight, other.srcPixelFormat)
{
}

VideoRescaler::VideoRescaler(VideoRescaler &&other)
    : VideoRescaler()
{
    swap(other);
}

VideoRescaler::~VideoRescaler()
{
    if (context)
        sws_freeContext(context);
}

void VideoRescaler::swap(VideoRescaler &other) noexcept
{
    using std::swap;
    swap(dstWidth,       other.dstWidth);
    swap(dstHeight,      other.dstHeight);
    swap(dstPixelFormat, other.dstPixelFormat);
    swap(srcWidth,       other.srcWidth);
    swap(srcHeight,      other.srcHeight);
    swap(srcPixelFormat, other.srcPixelFormat);
    swap(context,        other.context);
}

VideoRescaler& VideoRescaler::operator=(const VideoRescaler &rhs)
{
    if (this != &rhs)
        VideoRescaler(rhs).swap(*this);
    return *this;
}

VideoRescaler& VideoRescaler::operator=(VideoRescaler &&rhs)
{
    if (this != &rhs) {
        swap(rhs);
        VideoRescaler().swap(rhs);
    }
    return *this;
}


int32_t VideoRescaler::rescale(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame)
{
    if (!context)
    {
        cout << "Context does not allocated\n";
        return -1;
    }

    if (dstFrame->getWidth() != dstWidth)
    {
        cout << "Destination width does not match expected value\n";
        return -1;
    }

    if (dstFrame->getHeight() != dstHeight)
    {
        cout << "Destination height does not match expected value\n";
        return -1;
    }

    if (dstFrame->getPixelFormat() != dstPixelFormat)
    {
        cout << "Destination pixel format does not math expected value\n";
        return -1;
    }

    if (srcFrame->getWidth() != srcWidth)
    {
        cout << "Destination width does not match expected value\n";
        return -1;
    }

    if (srcFrame->getHeight() != srcHeight)
    {
        cout << "Destination height does not match expected value\n";
        return -1;
    }

    if (srcFrame->getPixelFormat() != srcPixelFormat)
    {
        cout << "Destination pixel format does not math expected value\n";
        return -1;
    }

    dstFrame->setPts(srcFrame->getPts());

    AVFrame *inFrame = srcFrame->getAVFrame();
    AVFrame *outFrame = dstFrame->getAVFrame();

    const uint8_t* srcFrameData[AV_NUM_DATA_POINTERS] = {
        inFrame->data[0],
        inFrame->data[1],
        inFrame->data[2],
        inFrame->data[3]
    #if AV_NUM_DATA_POINTERS == 8
        ,
        inFrame->data[4],
        inFrame->data[5],
        inFrame->data[6],
        inFrame->data[7]
    #endif
    };

    int stat = -1;
    stat = sws_scale(context, srcFrameData, inFrame->linesize, 0, srcHeight,
                     outFrame->data, outFrame->linesize);

    dstFrame->setQuality(srcFrame->getQuality());
    dstFrame->setTimeBase(srcFrame->getTimeBase());
    dstFrame->setPts(srcFrame->getPts());
    dstFrame->setFakePts(srcFrame->getFakePts());
    dstFrame->setStreamIndex(srcFrame->getStreamIndex());
    dstFrame->setComplete(stat >= 0);

    return stat;
}

bool VideoRescaler::isValid() const
{
    return (context ? true : false);
}


} // ::av
