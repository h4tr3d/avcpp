#include "frame.h"

using namespace std;

extern "C" {
#include <libavutil/version.h>
#include <libavutil/imgutils.h>
#include <libavutil/avassert.h>
}


namespace {

#if LIBAVUTIL_VERSION_INT <= AV_VERSION_INT(52,48,101)

#define CHECK_CHANNELS_CONSISTENCY(frame) \
    av_assert2(!(frame)->channel_layout || \
               (frame)->channels == \
               av_get_channel_layout_nb_channels((frame)->channel_layout))

int frame_copy_video(AVFrame *dst, const AVFrame *src)
{
    const uint8_t *src_data[4];
    int i, planes;

    if (dst->width  < src->width ||
        dst->height < src->height)
        return AVERROR(EINVAL);

    planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(dst->format));
    for (i = 0; i < planes; i++)
        if (!dst->data[i] || !src->data[i])
            return AVERROR(EINVAL);

    memcpy(src_data, src->data, sizeof(src_data));
    av_image_copy(dst->data, dst->linesize,
                  src_data, src->linesize,
                  static_cast<AVPixelFormat>(dst->format), src->width, src->height);

    return 0;
}

int frame_copy_audio(AVFrame *dst, const AVFrame *src)
{
    int planar   = av_sample_fmt_is_planar(static_cast<AVSampleFormat>(dst->format));
    int channels = dst->channels;
    int planes   = planar ? channels : 1;
    int i;

    if (dst->nb_samples     != src->nb_samples ||
        dst->channels       != src->channels ||
        dst->channel_layout != src->channel_layout)
        return AVERROR(EINVAL);

    CHECK_CHANNELS_CONSISTENCY(src);

    for (i = 0; i < planes; i++)
        if (!dst->extended_data[i] || !src->extended_data[i])
            return AVERROR(EINVAL);

    av_samples_copy(dst->extended_data, src->extended_data, 0, 0,
                    dst->nb_samples, channels, static_cast<AVSampleFormat>(dst->format));

    return 0;
}

int av_frame_copy(AVFrame *dst, const AVFrame *src)
{
    if (dst->format != src->format || dst->format < 0)
        return AVERROR(EINVAL);

    if (dst->width > 0 && dst->height > 0)
        return frame_copy_video(dst, src);
    else if (dst->nb_samples > 0 && dst->channel_layout)
        return frame_copy_audio(dst, src);

    return AVERROR(EINVAL);
}

#undef CHECK_CHANNELS_CONSISTENCY

#endif // LIBAVUTIL_VERSION_INT <= 53.5.0 (ffmpeg 2.2)

} // anonymous namespace

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
