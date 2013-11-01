#include <boost/format.hpp>

#include "frame.h"

using namespace std;

namespace av
{

Frame::Frame()
    : frame(0),
      timeBase(AV_TIME_BASE_Q),
      streamIndex(0),
      isCompleteFlag(false),
      fakePts(AV_NOPTS_VALUE)
{
    allocFrame();
}


Frame::~Frame()
{
    avcodec_free_frame(&frame);
}


void Frame::initFromAVFrame(const AVFrame *frame)
{
    if (!frame)
    {
        std::cout << "Try init from NULL frame" << std::endl;
        return;
    }

    *(this->frame) = *(frame);

    this->frame->extended_data = 0;
    this->frame->thread_opaque = 0;
    this->frame->owner         = 0;
    this->frame->hwaccel_picture_private = 0;
    this->frame->ref_index[0]  = 0;
    this->frame->ref_index[1]  = 0;
    this->frame->dct_coeff     = 0;
    this->frame->pan_scan      = 0;
    this->frame->opaque        = this;
    this->frame->qscale_table  = 0;
    this->frame->mbskip_table  = 0;
    this->frame->mb_type       = 0;

    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i)
    {
        this->frame->data[i] = 0;
        this->frame->linesize[i] = 0;
        this->frame->base[i] = 0;
        this->frame->error[i] = 0;
    }

    setupDataPointers(frame);
}


int64_t Frame::getPts() const
{
    return (frame ? frame->pts : AV_NOPTS_VALUE);
}

void Frame::setPts(int64_t pts)
{
    if (frame)
    {
        frame->pts = pts;
        setFakePts(pts);
    }
}

int64_t Frame::getBestEffortTimestamp() const
{
    return (frame ? frame->best_effort_timestamp : AV_NOPTS_VALUE);
}

int64_t Frame::getFakePts() const
{
    return fakePts;
}

void Frame::setFakePts(int64_t pts)
{
    fakePts = pts;
}

void Frame::setTimeBase(const Rational &value)
{
    if (timeBase == value)
        return;

    int64_t rescaledPts          = AV_NOPTS_VALUE;
    int64_t rescaledFakePts      = AV_NOPTS_VALUE;
    int64_t rescaledBestEffortTs = AV_NOPTS_VALUE;

    if (frame)
    {
        if (timeBase != Rational() && value != Rational())
        {
            if (frame->pts != AV_NOPTS_VALUE)
                rescaledPts = timeBase.rescale(frame->pts, value);

            if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                rescaledBestEffortTs = timeBase.rescale(frame->best_effort_timestamp, value);

            if (fakePts != AV_NOPTS_VALUE)
                rescaledFakePts = timeBase.rescale(fakePts, value);
        }
        else
        {
            rescaledPts          = frame->pts;
            rescaledFakePts      = fakePts;
            rescaledBestEffortTs = frame->best_effort_timestamp;
        }
    }

    timeBase = value;

    if (frame)
    {
        frame->pts                   = rescaledPts;
        frame->best_effort_timestamp = rescaledBestEffortTs;
        fakePts                      = rescaledFakePts;
    }
}

int Frame::getStreamIndex() const
{
    return streamIndex;
}

void Frame::setStreamIndex(int streamIndex)
{
    this->streamIndex = streamIndex;
}


const AVFrame *Frame::getAVFrame() const
{
    return frame;
}

AVFrame *Frame::getAVFrame()
{
    return frame;
}

const vector<uint8_t> &Frame::getFrameBuffer() const
{
    return frameBuffer;
}

void Frame::setComplete(bool isComplete)
{
    isCompleteFlag = isComplete;
}

void Frame::dump()
{
    av_hex_dump(stdout, frameBuffer.data(), frameBuffer.size());
}

Frame &Frame::operator =(const AVFrame *frame)
{
    if (this->frame && frame)
        initFromAVFrame(frame);
    return *this;
}

Frame &Frame::operator =(const Frame &frame)
{
    if (this->frame)
    {
        initFromAVFrame(frame.getAVFrame());
        timeBase       = frame.timeBase;
        isCompleteFlag = frame.isCompleteFlag;
        setPts(frame.getPts());
        fakePts        = frame.fakePts;
    }
    return *this;
}


void Frame::allocFrame()
{
    frame = avcodec_alloc_frame();
    frame->opaque = this;
}


} // ::av
