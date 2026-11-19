#pragma once

/**
 * Buffersink filter: frames leave the graph here.
 *
 * @file
 * @ingroup filter
 */

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

/**
 * Exit point of a filter graph.
 *
 * Wraps the "buffersink" or "abuffersink" filter: frames or samples leave the
 * graph here. A sink can produce zero, one or several outputs after a single
 * push, so reading in a loop until it returns false is the normal pattern.
 */
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
    /**
     * Wrap a sink context created by a graph.
     *
     * @param ctx the filter context to wrap
     * @param ec  holder for the error code; a non-sink filter is reported
     */
    explicit BufferSinkFilterContext(const FilterContext& ctx, OptionalErrorCode ec = throws());

    /**
     * Point the wrapper at another sink context.
     *
     * @param ctx the filter context to wrap
     * @param ec  holder for the error code
     */
    void assign(const FilterContext &ctx, OptionalErrorCode ec = throws());
    /**
     * Assign from a filter context.
     *
     * @param ctx the filter context to wrap
     *
     * @return this wrapper
     */
    BufferSinkFilterContext& operator=(const FilterContext &ctx);


    /**
     * Read one frame with explicit `AV_BUFFERSINK_FLAG_*` flags.
     *
     * @param frame receives the frame; left invalid when nothing is available
     * @param flags the sink flags
     * @param ec    holder for the error code
     *
     * @return `false` when no frame is available right now
     */
    bool getVideoFrame(VideoFrame &frame, int flags, OptionalErrorCode ec = throws());
    /**
     * Read one frame from the graph.
     *
     * @param frame receives the frame; left invalid when nothing is available
     * @param ec    holder for the error code
     *
     * @return `false` when no frame is available right now
     */
    bool getVideoFrame(VideoFrame &frame, OptionalErrorCode ec = throws());

    /**
     * Read one buffer of samples with explicit sink flags.
     *
     * @param samples receives the samples; left invalid when nothing is available
     * @param flags   the sink flags
     * @param ec      holder for the error code
     *
     * @return `false` when no samples are available right now
     */
    bool getAudioFrame(AudioSamples &samples, int flags, OptionalErrorCode ec = throws());
    /**
     * Read one buffer of samples from the graph.
     *
     * @param samples receives the samples; left invalid when nothing is available
     * @param ec      holder for the error code
     *
     * @return `false` when no samples are available right now
     */
    bool getAudioFrame(AudioSamples &samples, OptionalErrorCode ec = throws());
    /**
     * Read an exact number of samples.
     *
     * @param samples     receives the samples
     * @param samplesCount number of samples per channel to read
     * @param ec          holder for the error code
     *
     * @return `false` when the graph cannot provide that many samples right now
     */
    bool getAudioSamples(AudioSamples &samples, size_t samplesCount, OptionalErrorCode ec = throws());

    /**
     * Ask the sink to deliver frames of a fixed size.
     *
     * @param size samples per channel per output buffer
     * @param ec   holder for the error code
     */
    void     setFrameSize(unsigned size, OptionalErrorCode ec = throws());
    /**
     * Frame rate the sink reports.
     *
     * @param ec holder for the error code
     *
     * @return frames per second, or an empty value when the sink does not know it
     */
    Rational frameRate(OptionalErrorCode ec = throws());

    /**
     * Media type a filter would accept as a sink.
     *
     * @param filter the filter descriptor to inspect
     *
     * @return Video, Audio or Unknown
     */
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
