#pragma once

/**
 * Filter graph construction and configuration.
 *
 * @file
 * @ingroup filter
 */

#include <map>
#include <memory>

#include "avcpp/ffmpeg.h"
#include "avcpp/averror.h"

#include "filtercontext.h"
#include "buffersink.h"
#include "buffersrc.h"

#if AVCPP_HAS_AVFILTER

namespace av {

/**
 * A filter graph: filters, the links between them, and their configuration.
 *
 * Build it in one of two ways: parse() a textual description such as
 * "scale=320:240,format=yuv420p", or add filters by hand with createFilter()
 * and link() them. config() negotiates the formats and allocates the internals;
 * nothing can be pushed before it succeeds.
 *
 * The graph owns every context created through it, so contexts must not outlive
 * the graph.
 */
class FilterGraph : FFWrapperPtr<AVFilterGraph>
{
    friend class FilterInOut;

public:
    /**
     * Create an empty graph.
     */
    FilterGraph();
    /**
     * Create an empty graph from a nullptr tag; the graph starts empty.
     */
    FilterGraph(decltype(nullptr));
    ~FilterGraph();

    FilterGraph(const FilterGraph&) = delete;
    void operator=(const FilterGraph&) = delete;

    // Context getters/setters
    /**
     * Whether the graph has an FFmpeg context.
     *
     * @return `true` when the graph can accept filters
     */
    bool        isValid();
    //void        setScaleSwsOptions(const std::string& opts);
    /**
     * Number of filters in the graph.
     *
     * @return the filter count
     */
    int         filtersCount() const;
    /**
     * Enable automatic format conversion between linked filters.
     *
     * @param flags `AVFILTER_AUTO_CONVERT_*` bits
     */
    void        setAutoConvert(unsigned flags);

    // Public API

    /**
     * Filter instance by name.
     *
     * @param name the instance name given when it was created
     * @param ec   holder for the error code
     *
     * @return the context, or an invalid one when no such filter exists
     */
    FilterContext filter(const std::string& name, OptionalErrorCode ec = throws());
    /**
     * Filter instance by position.
     *
     * @param idx zero-based index into the graph
     * @param ec  holder for the error code
     *
     * @return the context, or an invalid one when the index is out of range
     */
    FilterContext filter(unsigned idx, OptionalErrorCode ec = throws());

    /**
     * Add a filter without initialising it.
     *
     * Initialise the returned context with init() before linking it.
     *
     * @param filter the filter descriptor to instantiate
     * @param name   name of the instance inside the graph
     * @param ec     holder for the error code
     *
     * @return the new context, or an invalid one on failure
     */
    FilterContext allocFilter(const Filter &filter, const std::string &name, OptionalErrorCode ec = throws());
    /**
     * Add a filter and initialise it with its arguments.
     *
     * @param filter     the filter descriptor to instantiate
     * @param filterName name of the instance inside the graph
     * @param filterArgs the filter-specific argument string
     * @param ec         holder for the error code
     *
     * @return the new context, or an invalid one on failure
     */
    FilterContext createFilter(const Filter &filter,
                               const std::string& filterName,
                               const std::string& filterArgs,
                               OptionalErrorCode ec = throws());

    /**
     * Build the graph from a textual description with explicit endpoints.
     *
     * The description names only the filters between the endpoints, for example
     * "scale=320:240,split [s0][s1]; [s0] palettegen [p]; [s1][p] paletteuse".
     *
     * @param graphDescription the filter chain
     * @param srcFilterCtx     the source context to start from
     * @param sinkFilterCtx    the sink context to end at
     * @param ec               holder for the error code
     */
    void parse(const std::string &graphDescription,
               FilterContext     &srcFilterCtx,
               FilterContext     &sinkFilterCtx,
               OptionalErrorCode  ec = throws());

    /**
     * Build the graph from a textual description.
     *
     * @param graphDescription the filter chain, including its buffer and
     *                         buffersink endpoints
     * @param ec               holder for the error code
     */
    void parse(const std::string &graphDescription, OptionalErrorCode  ec = throws());

    /**
     * Negotiate formats and allocate the graph.
     *
     * Must be called after the filters are linked and before anything is
     * pushed; a format mismatch is reported here rather than later.
     *
     * @param ec holder for the error code
     */
    void config(OptionalErrorCode ec = throws());

    /**
     * Describe the graph.
     *
     * @param doPrint `true` also writes the description through the FFmpeg log
     * @param options options for `avfilter_graph_dump()`; may be empty
     *
     * @return the textual description of the graph
     */
    std::string dump(bool doPrint = true, const std::string& options = std::string());

    /**
     * The source endpoint the graph was parsed with.
     *
     * @param ec holder for the error code
     *
     * @return the source context, or an invalid one when the graph has none
     */
    BufferSrcFilterContext  bufferSrcFilter(OptionalErrorCode ec = throws());
    /**
     * The sink endpoint the graph was parsed with.
     *
     * @param ec holder for the error code
     *
     * @return the sink context, or an invalid one when the graph has none
     */
    BufferSinkFilterContext bufferSinkFilter(OptionalErrorCode ec = throws());


private:

private:

    BufferSrcFilterContext  m_bufferSrc;
    bool                    m_bufferSrcSearchDone = false;
    BufferSinkFilterContext m_bufferSink;
    bool                    m_bufferSinkSearchDone = false;

    bool m_configured = false;
};

} // namespace av

#endif // if AVCPP_HAS_AVFILTER
