#include <iostream>

#include <boost/format.hpp>

#include "videoframe.h"

namespace av {

VideoFrame::VideoFrame()
{
}

VideoFrame::VideoFrame(PixelFormat pixelFormat, int width, int height)
{
    init(pixelFormat, width, height);
    int size = getSize();
    if (size > 0)
        frameBuffer.resize(size);
    avpicture_fill(reinterpret_cast<AVPicture*>(frame), frameBuffer.data(), pixelFormat, width, height);
    timeBase = Rational(AV_TIME_BASE_Q);
}

VideoFrame::VideoFrame(const vector<uint8_t> &data, PixelFormat pixelFormat, int width, int height)
{
    init(pixelFormat, width, height);
    // TODO: add check for data size
    //int size = getSize();
    frameBuffer = data;
    avpicture_fill(reinterpret_cast<AVPicture*>(frame), frameBuffer.data(), pixelFormat, width, height);
    timeBase = Rational(AV_TIME_BASE_Q);
}

VideoFrame::VideoFrame(const AVFrame *frame)
{
    initFromAVFrame(frame);
    timeBase = Rational(AV_TIME_BASE_Q);
    isCompleteFlag = true;
}

VideoFrame::VideoFrame(const VideoFrame &frame)
{
    initFromAVFrame(frame.getAVFrame());
    timeBase       = frame.getTimeBase();
    isCompleteFlag = frame.isComplete();
    streamIndex    = frame.getStreamIndex();
}

VideoFrame::~VideoFrame()
{
}

PixelFormat VideoFrame::getPixelFormat() const
{
    return (frame ? (PixelFormat)frame->format : PIX_FMT_NONE);
}

void VideoFrame::setPixelFormat(PixelFormat pixFmt)
{
    if (frame)
    {
        frame->format = pixFmt;
    }
}

int VideoFrame::getWidth() const
{
    return (frame ? frame->width : -1);
}

int VideoFrame::getHeight() const
{
    return (frame ? frame->height : -1);
}

bool VideoFrame::isKeyFrame() const
{
    return (frame ? frame->key_frame : false);
}

void VideoFrame::setKeyFrame(bool isKey)
{
    if (frame)
    {
        frame->key_frame = isKey;
    }
}

int VideoFrame::getQuality() const
{
    return (frame ? frame->quality : FF_LAMBDA_MAX);
}

void VideoFrame::setQuality(int quality)
{
    if (!frame)
        return;

    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    frame->quality = quality;
}

AVPictureType VideoFrame::getPictureType() const
{
    return (frame ? frame->pict_type : AV_PICTURE_TYPE_NONE);
}

void VideoFrame::setPictureType(AVPictureType type)
{
    if (frame)
    {
        frame->pict_type = type;
    }
}

const AVPicture &VideoFrame::getPicture() const
{
    return *(reinterpret_cast<AVPicture*>(frame));
}

int VideoFrame::getSize() const
{
    int retval = -1;
    if (frame && frame->format >= 0 && frame->width >=0 && frame->height >= 0)
    {
        retval = avpicture_get_size((PixelFormat)frame->format, frame->width, frame->height);
    }

    return retval;
}

bool VideoFrame::isValid() const
{
    if (frame &&
        frame->format >= 0 &&
        frame->width >= 0 &&
        frame->height >= 0 &&
        frameBuffer.size() == getSize() &&
        frame->data[0] == frameBuffer.data())
    {
        return true;
    }

    return false;
}

boost::shared_ptr<Frame> VideoFrame::clone()
{
    FramePtr frame(new VideoFrame(*this));
    return frame;
}

void VideoFrame::init(PixelFormat pixelFormat, int width, int height)
{
    frame->format = pixelFormat;
    frame->width = width;
    frame->height = height;
    setKeyFrame(true);
}

void VideoFrame::setupDataPointers(const AVFrame *frame)
{
    ssize_t size = -1;
    if (frame->format >= 0 && frame->width >= 0 && frame->height >= 0)
    {
        size = avpicture_get_size((PixelFormat)frame->format, frame->width, frame->height);
    }

    // This is error....
    if (size < 0)
    {
        std::cout << boost::format("Can't allocate memory for video frame data: "
                                   "empty picture (w:%d, h:%d, fmt:%d)") %
                     frame->width %
                     frame->height %
                     frame->format
                  << std::endl;

        return;
    }


    if (frameBuffer.size() < (size_t)size)
    {
        frameBuffer.resize(size);
    }

    //frameBuffer = vector<uint8_t>(size);

    uint8_t* buffer = (uint8_t*)frameBuffer.data();
    if (frame->data[0])
    {
        if (buffer != frame->data[0])
        {
            // Non-optimized copy
            //std::copy(frame->data[0], frame->data[0] + size, frameBuffer.begin());
            avpicture_fill(reinterpret_cast<AVPicture*>(this->frame),
                           buffer,
                           (PixelFormat)frame->format,
                           frame->width,
                           frame->height);
            av_picture_copy(reinterpret_cast<AVPicture*>(this->frame),
                            reinterpret_cast<const AVPicture*>(frame),
                            (PixelFormat)frame->format,
                            frame->width,
                            frame->height);
        }

        //this->frame->key_frame = frame->key_frame;
    }
    else
    {
        // Error...
        std::cout << "Can't copy data from frame: src frame has no data to copy\n";
        return;
    }
}


} // namespace av
