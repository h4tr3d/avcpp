#include <cassert>

#include "buffersink.h"

using namespace std;

namespace av {


BufferSinkFilterContext::BufferSinkFilterContext(const FilterContext &ctx, error_code &ec)
{
    assign(ctx, ec);
}

void BufferSinkFilterContext::assign(const FilterContext &ctx, error_code &ec)
{
    clear_if(ec);
    m_type = checkFilter(ctx.filter());
    if (m_type == FilterMediaType::Unknown) {
        throws_if(ec, Errors::IncorrectBufferSinkFilter);
        return;
    }
    m_sink = ctx;
}

BufferSinkFilterContext &BufferSinkFilterContext::operator=(const FilterContext &ctx)
{
    assign(ctx);
    return *this;
}

bool BufferSinkFilterContext::getVideoFrame(VideoFrame &frame, int flags, error_code &ec)
{
    if (m_type != FilterMediaType::Video) {
        throws_if(ec, Errors::IncorrectBufferSinkMediaType);
        return false;
    }

    return getFrame(frame.raw(), flags, ec);
}

bool BufferSinkFilterContext::getVideoFrame(VideoFrame &frame, error_code &ec)
{
    return getVideoFrame(frame, 0, ec);
}

bool BufferSinkFilterContext::getAudioFrame(AudioSamples &samples, int flags, error_code &ec)
{
    if (m_type != FilterMediaType::Audio) {
        throws_if(ec, Errors::IncorrectBufferSinkMediaType);
        return false;
    }
    return getFrame(samples.raw(), flags, ec);
}

bool BufferSinkFilterContext::getAudioFrame(AudioSamples &samples, error_code &ec)
{
    return getAudioFrame(samples, 0, ec);
}

bool BufferSinkFilterContext::getAudioSamples(AudioSamples &samples, size_t samplesCount, error_code &ec)
{
    if (m_type != FilterMediaType::Audio) {
        throws_if(ec, Errors::IncorrectBufferSinkMediaType);
        return false;
    }
    return getSamples(samples.raw(), samplesCount, ec);
}

void BufferSinkFilterContext::setFrameSize(unsigned size, error_code &ec)
{
    clear_if(ec);
    static_assert(LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(3,17,100),
                  "BufferSink set frame size functionality does not present on FFmpeg prior 1.0");
    if (!m_sink) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (m_type != FilterMediaType::Audio) {
        throws_if(ec, Errors::IncorrectBufferSinkMediaType);
        return;
    }

    av_buffersink_set_frame_size(m_sink.raw(), size);
}

Rational BufferSinkFilterContext::frameRate(error_code &ec)
{
    clear_if(ec);
    static_assert(LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(3,17,100),
                  "BufferSink get frame rate functionality does not present on FFmpeg prior 1.0");

    if (!m_sink) {
        throws_if(ec, Errors::Unallocated);
        return Rational();
    }

    if (m_type != FilterMediaType::Video) {
        throws_if(ec, Errors::IncorrectBufferSinkMediaType);
        return Rational();
    }

    return av_buffersink_get_frame_rate(m_sink.raw());
}

FilterMediaType BufferSinkFilterContext::checkFilter(const Filter &filter) noexcept
{
    if (filter) {
        if (filter.name() == "buffersink" || filter.name() == "ffbuffersink")
            return FilterMediaType::Video;
        else if (filter.name() == "abuffersink" || filter.name() == "ffabuffersink")
            return FilterMediaType::Audio;
    }
    return FilterMediaType::Unknown;
}

bool BufferSinkFilterContext::getFrame(AVFrame *frame, int flags, error_code &ec)
{
    clear_if(ec);
    if (!m_sink) {
        throws_if(ec, Errors::Unallocated);
        return false;
    }

    if (m_req == ReqGetSamples) {
        throws_if(ec, Errors::MixBufferSinkAccess);
        return false;
    }

    m_req = ReqGetFrame;

    int sts = av_buffersink_get_frame_flags(m_sink.raw(), frame, flags);
    if (sts < 0) {
        if (sts == AVERROR_EOF || sts == AVERROR(EAGAIN)) {
            if (&ec != &throws()) {
                ec = make_ffmpeg_error(sts);
            }
        } else {
            throws_if(ec, sts, ffmpeg_category());
        }
        return false;
    }
    return true;
}

bool BufferSinkFilterContext::getSamples(AVFrame *frame, int nbSamples, error_code &ec)
{
    clear_if(ec);
    if (!m_sink) {
        throws_if(ec, Errors::Unallocated);
        return false;
    }

    if (m_req == ReqGetFrame) {
        throws_if(ec, Errors::MixBufferSinkAccess);
        return false;
    }

    m_req = ReqGetSamples;

    int sts = av_buffersink_get_samples(m_sink.raw(), frame, nbSamples);
    if (sts < 0) {
        if (sts == AVERROR_EOF || sts == AVERROR(EAGAIN)) {
            if (&ec != &throws()) {
                ec = make_ffmpeg_error(sts);
            }
        } else {
            throws_if(ec, sts, ffmpeg_category());
        }
        return false;
    }
    return true;
}

} // namespace av
