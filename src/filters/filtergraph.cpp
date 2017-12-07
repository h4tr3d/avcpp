#define __STDC_FORMAT_MACROS 1

#include <list>
#include <sstream>
#include <cassert>

#include "avutils.h"
#include "filtergraph.h"
//#include "filterinout.h"
#include "avlog.h"

using namespace std;

namespace av {

FilterGraph::FilterGraph()
{
    m_raw = avfilter_graph_alloc();
}

FilterGraph::FilterGraph(decltype(nullptr))
{
}

FilterGraph::~FilterGraph()
{
    avfilter_graph_free(&m_raw);
}

bool FilterGraph::isValid()
{
    return !isNull();
}

int FilterGraph::filtersCount() const
{
    return (m_raw ? m_raw->nb_filters : 0);
}

void FilterGraph::setAutoConvert(unsigned flags)
{
    if (m_raw) {
        avfilter_graph_set_auto_convert(m_raw, flags);
    }
}


FilterContext FilterGraph::filter(const string &name, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return FilterContext();
    }

    AVFilterContext *ctx = avfilter_graph_get_filter(m_raw, name.c_str());
    if (!ctx)
        return FilterContext();

    return FilterContext(ctx);
}

FilterContext FilterGraph::filter(unsigned idx, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return FilterContext();
    }

    if (idx < m_raw->nb_filters) {
        throws_if(ec, Errors::OutOfRange);
        return FilterContext();
    }

    AVFilterContext *ctx = m_raw->filters[idx];
    if (!ctx) {
        throws_if(ec, Errors::InvalidArgument);
        return FilterContext();
    }

    return FilterContext(ctx);

}

FilterContext FilterGraph::allocFilter(const Filter &filter, const std::string &name, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return FilterContext();
    }

    AVFilterContext *ctx = avfilter_graph_alloc_filter(m_raw, filter.raw(), name.c_str());
    if (!ctx) {
        throws_if(ec, ENOMEM, system_category());
        return FilterContext();
    }
    
    return FilterContext(ctx);
}

FilterContext FilterGraph::createFilter(const Filter &filter, const string &filterName, const string &filterArgs, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!m_raw || filter.isNull()) {
        throws_if(ec, Errors::Unallocated);
        return FilterContext();
    }

    AVFilterContext *ctx = nullptr;

    int stat = avfilter_graph_create_filter(&ctx,
                                            filter.raw(),
                                            filterName.c_str(),
                                            filterArgs.empty() ? nullptr : filterArgs.c_str(),
                                            this,
                                            m_raw);
    if (stat < 0) {
        throws_if(ec, stat, ffmpeg_category());
        return FilterContext();
    }

    return FilterContext(ctx);
}

void FilterGraph::parse(const string &graphDescription,
                       FilterContext &srcFilterCtx,
                       FilterContext &sinkFilterCtx,
                       OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw || !srcFilterCtx || !sinkFilterCtx) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    auto srcf  = srcFilterCtx.raw();
    auto sinkf = sinkFilterCtx.raw();
    bool srcfFound = false;
    bool sinkfFound = false;

    for (size_t i = 0; i < (size_t)filtersCount(); ++i) {
        if (m_raw->filters[i] == srcf)
            srcfFound = true;
        if (m_raw->filters[i] == sinkf)
            sinkfFound = true;
        if (srcfFound && sinkfFound) // search completed
            break;
    }

    if (!srcfFound) {
        fflog(AV_LOG_ERROR, "Source filter does not present in filter graph");
        throws_if(ec, Errors::FilterNotInFilterGraph);
        return;
    }

    if (!sinkfFound) {
        fflog(AV_LOG_ERROR, "Sink filter does not present in filter graph");
        throws_if(ec, Errors::FilterNotInFilterGraph);
        return;
    }

    AVFilterInOut* inputs;
    AVFilterInOut* outputs;

    if (graphDescription.empty()) {
        fflog(AV_LOG_ERROR, "Empty graph description");
        throws_if(ec, Errors::FilterGraphDescriptionEmpty);
        return;
    } else {
        outputs = avfilter_inout_alloc();
        inputs = avfilter_inout_alloc();

        if (!outputs || !inputs) {
            throws_if(ec, errc::not_enough_memory);
            return;
        }

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = srcFilterCtx.raw();
        outputs->pad_idx    = 0;
        outputs->next       = 0;

        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = sinkFilterCtx.raw();
        inputs->pad_idx     = 0;
        inputs->next        = 0;

        int sts = avfilter_graph_parse(m_raw, graphDescription.c_str(), inputs, outputs, nullptr);
        if (sts < 0) {
            throws_if(ec, sts, ffmpeg_category());
            return;
        }
    }
}


void FilterGraph::config(OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    int sts = avfilter_graph_config(m_raw, nullptr);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }

    m_configured = true;
}

string FilterGraph::dump(bool doPrint, const string &options)
{
    string result;

    if (m_raw)
    {
        result = avfilter_graph_dump(m_raw, options.c_str());
        if (doPrint)
            clog << result;
    }

    return result;
}

BufferSrcFilterContext FilterGraph::bufferSrcFilter(OptionalErrorCode ec)
{
    if (m_bufferSrcSearchDone == false) {
        for (size_t i = 0; i < m_raw->nb_filters; ++i) {
            auto filt = m_raw->filters[i];
            if (BufferSrcFilterContext::checkFilter(filt->filter) != FilterMediaType::Unknown)
            {
                m_bufferSrc = BufferSrcFilterContext(filt, ec);
                break;
            }
        }
        m_bufferSrcSearchDone = true;
    }
    return m_bufferSrc;
}

BufferSinkFilterContext FilterGraph::bufferSinkFilter(OptionalErrorCode ec)
{
    if (m_bufferSinkSearchDone == false) {
        for (size_t i = 0; i < m_raw->nb_filters; ++i) {
            auto filt = m_raw->filters[i];
            if (BufferSinkFilterContext::checkFilter(filt->filter) != FilterMediaType::Unknown)
            {
                m_bufferSink = BufferSinkFilterContext(filt, ec);
                break;
            }
        }
        m_bufferSinkSearchDone = true;
    }
    return m_bufferSink;
}

} // namespace av
