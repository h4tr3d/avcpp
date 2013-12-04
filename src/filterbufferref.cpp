#include <cassert>

#include "filterbufferref.h"

namespace av {

FilterBufferRef::FilterBufferRef()
    : ref(0)
{
}

FilterBufferRef::FilterBufferRef(AVFilterBufferRef *ref, int mask)
    : ref(0)
{
    if (ref)
        this->ref = avfilter_ref_buffer(ref, mask);
}

FilterBufferRef::FilterBufferRef(const FilterBufferRef &ref, int mask)
    : ref(0)
{
    if (ref.ref)
        this->ref = avfilter_ref_buffer(ref.ref, mask);
}

FilterBufferRef::~FilterBufferRef()
{
    if (ref)
        avfilter_unref_buffer(ref);
}

FilterBufferRef &FilterBufferRef::operator =(const FilterBufferRef &rhs)
{
    if (ref != rhs.ref && rhs.ref)
    {
        AVFilterBufferRef *tmp = ref;
        ref = avfilter_ref_buffer(rhs.ref, 0);
        avfilter_unref_buffer(tmp);
    }

    return *this;
}

FilterBufferRef &FilterBufferRef::operator =(AVFilterBufferRef *rhs)
{
    if (ref != rhs && rhs)
    {
        AVFilterBufferRef *tmp = ref;
        ref = avfilter_ref_buffer(rhs, 0);
        avfilter_unref_buffer(tmp);
    }
    return *this;
}

void FilterBufferRef::copyProps(const FilterBufferRef &other)
{
    assert(ref);
    assert(other.ref);

    avfilter_copy_buffer_ref_props(ref, other.ref);
}

int FilterBufferRef::copyToFrame(FramePtr &dstFrame) const
{
    assert(ref);
    //assert(ref->type == AVMEDIA_TYPE_AUDIO || ref->type == AVMEDIA_TYPE_VIDEO);

    FramePtr result;

    if (ref->type == AVMEDIA_TYPE_AUDIO)
    {
        result = AudioSamplesPtr(new AudioSamples(
                                     (AVSampleFormat)ref->format,
                                     ref->audio->nb_samples,
                                     av_get_channel_layout_nb_channels(ref->audio->channel_layout),
                                     ref->audio->sample_rate));
    }
    else if (ref->type == AVMEDIA_TYPE_VIDEO)
    {
        result = VideoFramePtr(new VideoFrame(
                                   (PixelFormat)ref->format,
                                   ref->video->w,
                                   ref->video->h
                                   ));
    }
    else
    {
        clog << "Unsupported media type: (" << ref->type << ") " << av_get_media_type_string(ref->type) << endl;
        return -1;
    }

    int stat = avfilter_copy_buf_props(result->getAVFrame(), ref);
    if (stat >= 0)
    {
        if (ref->type == AVMEDIA_TYPE_AUDIO)
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
    assert(ref);
    return ref->type;
}

void FilterBufferRef::reset(AVFilterBufferRef *other)
{
    if (ref != other && other)
    {
        AVFilterBufferRef *tmp = ref;
        ref = other;
        avfilter_unref_buffer(tmp);
    }
}

AVFilterBufferRef *FilterBufferRef::getAVFilterBufferRef()
{
    return ref;
}

bool FilterBufferRef::isValid() const
{
    return !!ref;
}


} // namespace av
