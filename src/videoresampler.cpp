#include "videoresampler.h"

namespace av
{

VideoResampler::VideoResampler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat, int srcWidth, int srcHeight, PixelFormat srcPixelFormat)
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

VideoResampler::~VideoResampler()
{
    if (context)
        sws_freeContext(context);
    context = 0;
}


int32_t VideoResampler::resample(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame)
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

bool VideoResampler::isValid() const
{
    return (context ? true : false);
}


} // ::av
