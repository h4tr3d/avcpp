#pragma once

#include "avcpp/avconfig.h"

#if AVCPP_HAS_AVFILTER

#include <stdint.h>
#include <memory>

#include "avcpp/ffmpeg.h"
#include "avcpp/rational.h"
#include "avcpp/averror.h"
#include "avcpp/frame.h"
#include "filter.h"
#include "filtercontext.h"

namespace av {

class BufferSinkFilterContext
{
    // To protect access
    enum {
        ReqUninited,
        ReqGetFrame,
        ReqGetSamples
    };

public:
    BufferSinkFilterContext() = default;
    explicit BufferSinkFilterContext(const FilterContext& ctx, OptionalErrorCode ec = throws());

    void assign(const FilterContext &ctx, OptionalErrorCode ec = throws());
    BufferSinkFilterContext& operator=(const FilterContext &ctx);


    bool getVideoFrame(VideoFrame &frame, int flags, OptionalErrorCode ec = throws());
    bool getVideoFrame(VideoFrame &frame, OptionalErrorCode ec = throws());

    bool getAudioFrame(AudioSamples &samples, int flags, OptionalErrorCode ec = throws());
    bool getAudioFrame(AudioSamples &samples, OptionalErrorCode ec = throws());
    bool getAudioSamples(AudioSamples &samples, size_t samplesCount, OptionalErrorCode ec = throws());

    void     setFrameSize(unsigned size, OptionalErrorCode ec = throws());
    Rational frameRate(OptionalErrorCode ec = throws());

    static FilterMediaType checkFilter(const Filter& filter) noexcept;

private:
    // AVERROR_EOF and AVERROR(EAGAIN) never thrown, but can be returned via error_code
    // if ec == throws(), false returns silently
    bool getFrame(AVFrame *frame, int flags, OptionalErrorCode ec);
    bool getSamples(AVFrame *frame, int nbSamples, OptionalErrorCode ec);

private:
    FilterContext   m_sink;
    FilterMediaType m_type = FilterMediaType::Unknown;
    int             m_req  = ReqUninited;
};


} // namespace av

#endif