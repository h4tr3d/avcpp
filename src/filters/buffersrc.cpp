#include <cassert>

#include "buffersrc.h"

using namespace std;

namespace av {

BufferSrcFilterContext::BufferSrcFilterContext(const FilterContext &ctx, error_code &ec)
{
    assign(ctx, ec);
}

void BufferSrcFilterContext::assign(const FilterContext &ctx, error_code &ec)
{
    clear_if(ec);
    m_type = checkFilter(ctx.filter());
    if (m_type == FilterMediaType::Unknown) {
        throws_if(ec, Errors::IncorrectBufferSrcFilter);
        return;
    }
    m_src = ctx;
}

BufferSrcFilterContext &BufferSrcFilterContext::operator=(const FilterContext &ctx)
{
    assign(ctx);
    return *this;
}

void BufferSrcFilterContext::addVideoFrame(VideoFrame &frame, int flags, error_code &ec)
{
    if (m_type == FilterMediaType::Video) {
        addFrame(frame.raw(), flags, ec);
    } else {
        throws_if(ec, Errors::IncorrectBufferSrcMediaType);
    }
}

void BufferSrcFilterContext::addVideoFrame(VideoFrame &frame, error_code &ec)
{
    addVideoFrame(frame, 0, ec);
}

void BufferSrcFilterContext::writeVideoFrame(const VideoFrame &frame, error_code &ec)
{
    if (m_type == FilterMediaType::Video) {
        writeFrame(frame.raw(), ec);
    } else {
        throws_if(ec, Errors::IncorrectBufferSrcMediaType);
    }
}

void BufferSrcFilterContext::addAudioSamples(AudioSamples &samples, int flags, error_code &ec)
{
    if (m_type == FilterMediaType::Audio) {
        addFrame(samples.raw(), flags, ec);
    } else {
        throws_if(ec, Errors::IncorrectBufferSrcMediaType);
    }
}

void BufferSrcFilterContext::addAudioSamples(AudioSamples &samples, error_code &ec)
{
    addAudioSamples(samples, 0, ec);
}

void BufferSrcFilterContext::writeAudioSamples(const AudioSamples &samples, error_code &ec)
{
    if (m_type == FilterMediaType::Audio) {
        writeFrame(samples.raw(), ec);
    } else {
        throws_if(ec, Errors::IncorrectBufferSrcMediaType);
    }
}

size_t BufferSrcFilterContext::failedRequestsCount()
{
    if (m_src)
        return av_buffersrc_get_nb_failed_requests(m_src.raw());
    else
        return 0;
}

FilterMediaType BufferSrcFilterContext::checkFilter(const Filter &filter)
{
    if (filter) {
        if (filter.name() == "buffer")
            return FilterMediaType::Video;
        else if (filter.name() == "abuffer")
            return FilterMediaType::Audio;
    }
    return FilterMediaType::Unknown;
}

void BufferSrcFilterContext::addFrame(AVFrame *frame, int flags, error_code &ec)
{
    clear_if(ec);
    int sts = av_buffersrc_add_frame_flags(m_src.raw(), frame, flags);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
}

void BufferSrcFilterContext::writeFrame(const AVFrame *frame, error_code &ec)
{
    clear_if(ec);
    int sts = av_buffersrc_write_frame(m_src.raw(), frame);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
}

} // namespace av
