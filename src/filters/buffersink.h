#ifndef AV_BUFFERSINK_H
#define AV_BUFFERSINK_H

#include <stdint.h>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "filtercontext.h"
#include "filter.h"
#include "averror.h"

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
    explicit BufferSinkFilterContext(const FilterContext& ctx, std::error_code &ec = throws());

    void assign(const FilterContext &ctx, std::error_code &ec = throws());
    BufferSinkFilterContext& operator=(const FilterContext &ctx);


    bool getVideoFrame(VideoFrame &frame, int flags, std::error_code &ec = throws());
    bool getVideoFrame(VideoFrame &frame, std::error_code &ec = throws());

    bool getAudioFrame(AudioSamples &samples, int flags, std::error_code &ec = throws());
    bool getAudioFrame(AudioSamples &samples, std::error_code &ec = throws());
    bool getAudioSamples(AudioSamples &samples, size_t samplesCount, std::error_code &ec = throws());

    void     setFrameSize(unsigned size, std::error_code &ec = throws());
    Rational frameRate(std::error_code &ec = throws());

    static FilterMediaType checkFilter(const Filter& filter) noexcept;

private:
    // AVERROR_EOF and AVERROR(EAGAIN) never thrown, but can be returned via error_code
    // if ec == throws(), false returns silently
    bool getFrame(AVFrame *frame, int flags, std::error_code &ec);
    bool getSamples(AVFrame *frame, int nbSamples, std::error_code &ec);

private:
    FilterContext   m_sink;
    FilterMediaType m_type = FilterMediaType::Unknown;
    int             m_req  = ReqUninited;
};


} // namespace av

#endif // AV_BUFFERSINK_H
