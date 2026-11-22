#pragma once

/**
 * Source and sink filter contexts.
 *
 * @file
 * @ingroup filter
 */

#include <memory>

#include "avcpp/ffmpeg.h"
#include "avcpp/averror.h"
#include "filter.h"
//#include "filteropaque.h"
//#include "filtercontext_helper.h"

#if AVCPP_HAS_AVFILTER

namespace av {

/**
 * One filter instance inside a graph.
 *
 * The context holds the filter, its pads and its private options. It is created
 * by FilterGraph, initialised with init() and attached to other contexts with
 * link(); its lifetime is tied to the graph.
 */
class FilterContext : public FFWrapperPtr<AVFilterContext>
{
    friend class FilterGraph;

public:
    using FFWrapperPtr<AVFilterContext>::FFWrapperPtr;

    /**
     * Wrap an FFmpeg filter context.
     *
     * @param ctx the context; the graph owns it
     */
    FilterContext(AVFilterContext *ctx);
    FilterContext() = default;

    /**
     * Descriptor of the filter this context instantiates.
     *
     * @return the filter, or an empty one when the context is invalid
     */
    Filter            filter() const;
    /**
     * Name of this instance inside the graph.
     *
     * @return the name given when the context was created
     */
    std::string       name() const;

    /**
     * Number of input pads.
     *
     * @return the input count
     */
    size_t            inputsCount()  const;
    /**
     * Number of output pads.
     *
     * @return the output count
     */
    size_t            outputsCount() const;

    /**
     * Initialise the filter with its arguments.
     *
     * @param args the filter-specific argument string, for example
     *             "320:240:flags=lanczos" for scale
     * @param ec   holder for the error code
     */
    void init(const std::string &args, OptionalErrorCode ec = throws());
    /**
     * Release the filter instance.
     *
     * The graph owns the context, so this is only needed before the graph
     * itself is destroyed.
     */
    void free();

    /**
     * Connect one of this context's outputs to an input of another.
     *
     * @param srcPad    output pad index on this context
     * @param dstFilter the context to connect to
     * @param dstPad    input pad index on the destination
     * @param ec        holder for the error code
     */
    void link(unsigned srcPad, FilterContext& dstFilter, unsigned dstPad, OptionalErrorCode ec = throws());

    /**
     * Whether the context refers to a filter instance.
     *
     * @return `true` when the underlying AVFilterContext exists
     */
    operator bool() const;

protected:

};

} // namespace av

#endif // if AVCPP_HAS_AVFILTER
