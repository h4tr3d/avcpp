#ifndef AV_BUFFERSRC_H
#define AV_BUFFERSRC_H

#include "ffmpeg.h"
#include "filtercontext.h"
#include "filter.h"
#include "rational.h"
#include "frame.h"
#include "averror.h"

namespace av {

class BufferSrcFilterContext
{
public:
    BufferSrcFilterContext() = default;
    BufferSrcFilterContext(const FilterContext &ctx, OptionalErrorCode ec = throws());

    void assign(const FilterContext &ctx, OptionalErrorCode ec = throws());
    BufferSrcFilterContext& operator=(const FilterContext &ctx);

    void writeVideoFrame(const VideoFrame &frame, OptionalErrorCode ec = throws());
    void addVideoFrame(VideoFrame &frame, int flags, OptionalErrorCode ec = throws());
    void addVideoFrame(VideoFrame &frame, OptionalErrorCode ec = throws());

    void writeAudioSamples(const AudioSamples &samples, OptionalErrorCode ec = throws());
    void addAudioSamples(AudioSamples &samples, int flags, OptionalErrorCode ec = throws());
    void addAudioSamples(AudioSamples &samples, OptionalErrorCode ec = throws());

    size_t failedRequestsCount();

    static FilterMediaType checkFilter(const Filter& filter);

private:
    void addFrame(AVFrame *frame, int flags, OptionalErrorCode ec);
    void writeFrame(const AVFrame *frame, OptionalErrorCode ec);

private:
    FilterContext   m_src;
    FilterMediaType m_type = FilterMediaType::Unknown;
};

} // namespace av

#endif // AV_BUFFERSRC_H
