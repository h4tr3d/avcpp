#include <cassert>

#include "filterbufferref.h"

namespace av {

FilterBufferRef::FilterBufferRef()
    : m_ref(0)
{
}

FilterBufferRef::FilterBufferRef(AVFilterBufferRef *ref, int mask)
    : m_ref(0)
{
    if (ref)
        this->m_ref = avfilter_ref_buffer(ref, mask);
}

FilterBufferRef::FilterBufferRef(const FilterBufferRef &ref, int mask)
    : m_ref(0)
{
    if (ref.m_ref)
        this->m_ref = avfilter_ref_buffer(ref.m_ref, mask);
}

FilterBufferRef::~FilterBufferRef()
{
    if (m_ref)
        avfilter_unref_buffer(m_ref);
}

FilterBufferRef &FilterBufferRef::operator =(const FilterBufferRef &rhs)
{
    if (m_ref != rhs.m_ref && rhs.m_ref)
    {
        AVFilterBufferRef *tmp = m_ref;
        m_ref = avfilter_ref_buffer(rhs.m_ref, 0);
        avfilter_unref_buffer(tmp);
    }

    return *this;
}

FilterBufferRef &FilterBufferRef::operator =(AVFilterBufferRef *rhs)
{
    if (m_ref != rhs && rhs)
    {
        AVFilterBufferRef *tmp = m_ref;
        m_ref = avfilter_ref_buffer(rhs, 0);
        avfilter_unref_buffer(tmp);
    }
    return *this;
}

void FilterBufferRef::copyProps(const FilterBufferRef &other)
{
    assert(m_ref);
    assert(other.m_ref);

    avfilter_copy_buffer_ref_props(m_ref, other.m_ref);
}

int FilterBufferRef::copyToFrame(FramePtr &dstFrame) const
{
    assert(m_ref);
    //assert(ref->type == AVMEDIA_TYPE_AUDIO || ref->type == AVMEDIA_TYPE_VIDEO);

    FramePtr result;

    if (m_ref->type == AVMEDIA_TYPE_AUDIO)
    {
        result = AudioSamplesPtr(new AudioSamples(
                                     (AVSampleFormat)m_ref->format,
                                     m_ref->audio->nb_samples,
                                     av_get_channel_layout_nb_channels(m_ref->audio->channel_layout),
                                     m_ref->audio->sample_rate));
    }
    else if (m_ref->type == AVMEDIA_TYPE_VIDEO)
    {
        result = VideoFramePtr(new VideoFrame(
                                   (PixelFormat)m_ref->format,
                                   m_ref->video->w,
                                   m_ref->video->h
                                   ));
    }
    else
    {
        clog << "Unsupported media type: (" << m_ref->type << ") " << av_get_media_type_string(m_ref->type) << endl;
        return -1;
    }

    int stat = avfilter_copy_buf_props(result->getAVFrame(), m_ref);
    if (stat >= 0)
    {
        if (m_ref->type == AVMEDIA_TYPE_AUDIO)
        {
            result->getAVFrame()->pts = AV_NOPTS_VALUE;
        }
        dstFrame = result;
    }

    return stat;
}

int FilterBufferRef::makeFromFrame(const AudioSamplesPtr &samples, int perms)
{
    assert(samples);

    AVFilterBufferRef *ref = avfilter_get_audio_buffer_ref_from_frame(samples->getAVFrame(), perms);
    if (ref)
    {
        reset(ref);
        return 0;
    }

    return -1;
}

int FilterBufferRef::makeFromFrame(const VideoFramePtr &frame, int perms)
{
    assert(frame);

    AVFilterBufferRef *ref = avfilter_get_video_buffer_ref_from_frame(frame->getAVFrame(), perms);
    if (ref)
    {
        reset(ref);
        return 0;
    }

    return -1;
}

AVMediaType FilterBufferRef::getMediaType() const
{
    assert(m_ref);
    return m_ref->type;
}

void FilterBufferRef::reset(AVFilterBufferRef *other)
{
    if (m_ref != other && other)
    {
        AVFilterBufferRef *tmp = m_ref;
        m_ref = other;
        avfilter_unref_buffer(tmp);
    }
}

AVFilterBufferRef *FilterBufferRef::getAVFilterBufferRef()
{
    return m_ref;
}

bool FilterBufferRef::isValid() const
{
    return !!m_ref;
}


} // namespace av
