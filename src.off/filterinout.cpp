#include "filterinout.h"

namespace av {

using namespace std;

FilterInOut::FilterInOut()
    : inout(0)
{
//    inout = avfilter_inout_alloc();
//    inout->next = 0;
}

FilterInOut::FilterInOut(AVFilterInOut *inout, const FilterGraphPtr &graph)
    : inout(inout),
      graph(graph)
{
}

FilterInOut::~FilterInOut()
{
    reset();
}

AVFilterInOut *FilterInOut::getAVFilterInOut()
{
    return inout;
}

void FilterInOut::reset(AVFilterInOut *other)
{
    inout = other;
}

std::string FilterInOut::getName() const
{
    return (inout && inout->name ? string(inout->name) : string());
}

int FilterInOut::getPadIndex() const
{
    return (inout ? inout->pad_idx : -1);
}

FilterContextPtr FilterInOut::getFilterContext() const
{
    return (inout && graph ? graph->getFilter(inout->filter_ctx) : FilterContextPtr());
}

void FilterInOut::setName(const string &name)
{
    if (inout)
    {
        if (inout->name)
        {
            av_freep(&inout->name);
        }

        inout->name = av_strdup(name.c_str());
    }
}

void FilterInOut::setPadIndex(int index)
{
    if (inout)
    {
        inout->pad_idx = index;
    }
}

void FilterInOut::setFilterContext(const FilterContextPtr &ctx)
{
    if (inout)
    {
        inout->filter_ctx = ctx->getAVFilterContext();
    }
}

FilterGraphPtr FilterInOut::getFilterGraph() const
{
    return graph;
}

void FilterInOut::setFilterGraph(const FilterGraphPtr &graph)
{
    this->graph = graph;
}

} // namespace av
