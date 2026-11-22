#pragma once

/**
 * Buffersrc filter: frames enter the graph here.
 *
 * @file
 * @ingroup filter
 */

#include "avcpp/avconfig.h"

#if AVCPP_HAS_AVFILTER

#include "avcpp/ffmpeg.h"
#include "avcpp/rational.h"
#include "avcpp/frame.h"
#include "avcpp/averror.h"
#include "filtercontext.h"
#include "filter.h"

namespace av {

/**
 * Entry point of a filter graph.
 *
 * Wraps the "buffer" or "abuffersrc" filter: frames or samples written here
 * enter the graph. The source is typed by the filter it wraps, and the typed
 * write functions refuse data of the other kind.
 */
class BufferSrcFilterContext
{
public:
    BufferSrcFilterContext() = default;
    /**
     * Wrap a source context created by a graph.
     *
     * @param ctx the filter context to wrap
     * @param ec  holder for the error code; a non-source filter is reported
     */
    BufferSrcFilterContext(const FilterContext &ctx, OptionalErrorCode ec = throws());

    /**
     * Point the wrapper at another source context.
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
    BufferSrcFilterContext& operator=(const FilterContext &ctx);

    /**
     * Write a video frame into the graph.
     *
     * The frame is referenced, not copied, and must stay valid until the graph
     * consumed it, which happens during the call.
     *
     * @param frame the frame to push
     * @param ec    holder for the error code
     */
    void writeVideoFrame(const VideoFrame &frame, OptionalErrorCode ec = throws());
    /**
     * Push a video frame with explicit `AV_BUFFERSRC_FLAG_*` flags.
     *
     * @param frame the frame to push
     * @param flags for example `AV_BUFFERSRC_FLAG_KEEP_REF` to keep the frame
     *              usable after the call
     * @param ec    holder for the error code
     */
    void addVideoFrame(VideoFrame &frame, int flags, OptionalErrorCode ec = throws());
    /**
     * Push a video frame into the graph.
     *
     * @param frame the frame to push
     * @param ec    holder for the error code
     */
    void addVideoFrame(VideoFrame &frame, OptionalErrorCode ec = throws());

    /**
     * Write audio samples into the graph.
     *
     * @param samples the samples to push
     * @param ec      holder for the error code
     */
    void writeAudioSamples(const AudioSamples &samples, OptionalErrorCode ec = throws());
    /**
     * Push audio samples with explicit flags.
     *
     * @param samples the samples to push
     * @param flags   `AV_BUFFERSRC_FLAG_*` bits
     * @param ec      holder for the error code
     */
    void addAudioSamples(AudioSamples &samples, int flags, OptionalErrorCode ec = throws());
    /**
     * Push audio samples into the graph.
     *
     * @param samples the samples to push
     * @param ec      holder for the error code
     */
    void addAudioSamples(AudioSamples &samples, OptionalErrorCode ec = throws());

    /**
     * How many pushes the graph refused.
     *
     * A refused push is EAGAIN: the graph wants its output drained first, which
     * is not an error but usually means the pull loop is missing.
     *
     * @return the number of refused pushes since the context was created
     */
    size_t failedRequestsCount();

    /**
     * Media type a filter would produce as a source.
     *
     * @param filter the filter descriptor to inspect
     *
     * @return Video, Audio or Unknown
     */
    static FilterMediaType checkFilter(const Filter& filter);

private:
    void addFrame(AVFrame *frame, int flags, OptionalErrorCode ec);
    void writeFrame(const AVFrame *frame, OptionalErrorCode ec);

private:
    FilterContext   m_src;
    FilterMediaType m_type = FilterMediaType::Unknown;
};

} // namespace av

#endif // if AVCPP_HAS_AVFILTER
