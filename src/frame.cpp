#include "frame.h"

using namespace std;

namespace av
{

Frame::Frame()
    : m_frame(0),
      m_timeBase(AV_TIME_BASE_Q),
      m_streamIndex(0),
      m_completeFlag(false),
      m_fakePts(AV_NOPTS_VALUE)
{
    allocFrame();
}


Frame::~Frame()
{
    if (!m_frame)
        av_frame_free(&m_frame);
}


void Frame::initFromAVFrame(const AVFrame *frame)
{
    if (!frame)
    {
        std::cout << "Try init from NULL frame" << std::endl;
        return;
    }

    // Setup pointers
    setupDataPointers(frame);

    // Copy frame
    av_frame_copy(m_frame, frame);
    av_frame_copy_props(m_frame, frame);
}


int64_t Frame::getPts() const
{
    return (m_frame ? m_frame->pts : AV_NOPTS_VALUE);
}

void Frame::setPts(int64_t pts)
{
    if (m_frame)
    {
        m_frame->pts = pts;
        setFakePts(pts);
    }
}

int64_t Frame::getBestEffortTimestamp() const
{
    return (m_frame ? m_frame->best_effort_timestamp : AV_NOPTS_VALUE);
}

int64_t Frame::getFakePts() const
{
    return m_fakePts;
}

void Frame::setFakePts(int64_t pts)
{
    m_fakePts = pts;
}

void Frame::setTimeBase(const Rational &value)
{
    if (m_timeBase == value)
        return;

    int64_t rescaledPts          = AV_NOPTS_VALUE;
    int64_t rescaledFakePts      = AV_NOPTS_VALUE;
    int64_t rescaledBestEffortTs = AV_NOPTS_VALUE;

    if (m_frame)
    {
        if (m_timeBase != Rational() && value != Rational())
        {
            if (m_frame->pts != AV_NOPTS_VALUE)
                rescaledPts = m_timeBase.rescale(m_frame->pts, value);

            if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
                rescaledBestEffortTs = m_timeBase.rescale(m_frame->best_effort_timestamp, value);

            if (m_fakePts != AV_NOPTS_VALUE)
                rescaledFakePts = m_timeBase.rescale(m_fakePts, value);
        }
        else
        {
            rescaledPts          = m_frame->pts;
            rescaledFakePts      = m_fakePts;
            rescaledBestEffortTs = m_frame->best_effort_timestamp;
        }
    }

    m_timeBase = value;

    if (m_frame)
    {
        m_frame->pts                   = rescaledPts;
        m_frame->best_effort_timestamp = rescaledBestEffortTs;
        m_fakePts                      = rescaledFakePts;
    }
}

int Frame::getStreamIndex() const
{
    return m_streamIndex;
}

void Frame::setStreamIndex(int streamIndex)
{
    this->m_streamIndex = streamIndex;
}


const AVFrame *Frame::getAVFrame() const
{
    return m_frame;
}

AVFrame *Frame::getAVFrame()
{
    return m_frame;
}

const vector<uint8_t> &Frame::getFrameBuffer() const
{
    return m_frameBuffer;
}

void Frame::setComplete(bool isComplete)
{
    m_completeFlag = isComplete;
}

void Frame::dump()
{
    av_hex_dump(stdout, m_frameBuffer.data(), m_frameBuffer.size());
}

Frame &Frame::operator =(const AVFrame *frame)
{
    if (m_frame && frame)
        initFromAVFrame(frame);
    return *this;
}

Frame &Frame::operator =(const Frame &frame)
{
    if (m_frame == frame.m_frame || this == &frame)
        return *this;

    if (this->m_frame)
    {
        initFromAVFrame(frame.getAVFrame());
        m_timeBase     = frame.m_timeBase;
        m_completeFlag = frame.m_completeFlag;
        setPts(frame.getPts());
        m_fakePts      = frame.m_fakePts;
    }
    return *this;
}


void Frame::allocFrame()
{
    m_frame = av_frame_alloc();
    m_frame->opaque = this;
}


} // ::av
