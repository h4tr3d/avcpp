#include <iostream>
#include <cstdio>

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
        m_frameBuffer.resize(size);
    avpicture_fill(reinterpret_cast<AVPicture*>(m_frame), m_frameBuffer.data(), pixelFormat, width, height);
    m_timeBase = Rational(AV_TIME_BASE_Q);
}

VideoFrame::VideoFrame(const vector<uint8_t> &data, PixelFormat pixelFormat, int width, int height)
{
    init(pixelFormat, width, height);
    // TODO: add check for data size
    //int size = getSize();
    m_frameBuffer = data;
    avpicture_fill(reinterpret_cast<AVPicture*>(m_frame), m_frameBuffer.data(), pixelFormat, width, height);
    m_timeBase = Rational(AV_TIME_BASE_Q);
}

VideoFrame::VideoFrame(const AVFrame *frame)
{
    initFromAVFrame(frame);
    m_timeBase = Rational(AV_TIME_BASE_Q);
    m_completeFlag = true;
}

VideoFrame::VideoFrame(const VideoFrame &frame)
{
    initFromAVFrame(frame.getAVFrame());
    m_timeBase     = frame.getTimeBase();
    m_completeFlag = frame.isComplete();
    m_streamIndex  = frame.getStreamIndex();
}

VideoFrame::~VideoFrame()
{
}

PixelFormat VideoFrame::getPixelFormat() const
{
    return (m_frame ? (PixelFormat)m_frame->format : PIX_FMT_NONE);
}

void VideoFrame::setPixelFormat(PixelFormat pixFmt)
{
    if (m_frame)
    {
        m_frame->format = pixFmt;
    }
}

int VideoFrame::getWidth() const
{
    return (m_frame ? m_frame->width : -1);
}

int VideoFrame::getHeight() const
{
    return (m_frame ? m_frame->height : -1);
}

bool VideoFrame::isKeyFrame() const
{
    return (m_frame ? m_frame->key_frame : false);
}

void VideoFrame::setKeyFrame(bool isKey)
{
    if (m_frame)
    {
        m_frame->key_frame = isKey;
    }
}

int VideoFrame::getQuality() const
{
    return (m_frame ? m_frame->quality : FF_LAMBDA_MAX);
}

void VideoFrame::setQuality(int quality)
{
    if (!m_frame)
        return;

    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    m_frame->quality = quality;
}

AVPictureType VideoFrame::getPictureType() const
{
    return (m_frame ? m_frame->pict_type : AV_PICTURE_TYPE_NONE);
}

void VideoFrame::setPictureType(AVPictureType type)
{
    if (m_frame)
    {
        m_frame->pict_type = type;
    }
}

const AVPicture &VideoFrame::getPicture() const
{
    return *(reinterpret_cast<AVPicture*>(m_frame));
}

int VideoFrame::getSize() const
{
    int retval = -1;
    if (m_frame && m_frame->format >= 0 && m_frame->width >=0 && m_frame->height >= 0)
    {
        retval = avpicture_get_size((PixelFormat)m_frame->format, m_frame->width, m_frame->height);
    }

    return retval;
}

bool VideoFrame::isValid() const
{
    if (m_frame &&
        m_frame->format >= 0 &&
        m_frame->width >= 0 &&
        m_frame->height >= 0 &&
        m_frameBuffer.size() == getSize() &&
        m_frame->data[0] == m_frameBuffer.data())
    {
        return true;
    }

    return false;
}

std::shared_ptr<Frame> VideoFrame::clone()
{
    FramePtr frame(new VideoFrame(*this));
    return frame;
}

void VideoFrame::init(PixelFormat pixelFormat, int width, int height)
{
    m_frame->format = pixelFormat;
    m_frame->width = width;
    m_frame->height = height;
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
        std::fprintf(stderr,
                     "Can't allocate memory for video frame data: "
                     "empty picture (w:%d, h:%d, fmt:%d)",
                     frame->width,
                     frame->height,
                     frame->format);

        return;
    }

    m_frame->format = frame->format;
    m_frame->width  = frame->width;
    m_frame->height = frame->height;

    if (m_frameBuffer.size() < (size_t)size)
    {
        m_frameBuffer.resize(size);
    }

    //uint8_t* buffer = (uint8_t*)m_frameBuffer.data();
    uint8_t* buffer = m_frameBuffer.data();
    if (frame->data[0])
    {
        if (buffer != frame->data[0])
        {

            avpicture_fill(reinterpret_cast<AVPicture*>(m_frame),
                           buffer,
                           (PixelFormat)frame->format,
                           frame->width,
                           frame->height);
#if 0
            av_picture_copy(reinterpret_cast<AVPicture*>(this->m_frame),
                            reinterpret_cast<const AVPicture*>(frame),
                            (PixelFormat)frame->format,
                            frame->width,
                            frame->height);
#endif
        }
    }
    else
    {
        // Error...
        std::cout << "Can't copy data from frame: src frame has no data to copy\n";
        return;
    }
}


} // namespace av
