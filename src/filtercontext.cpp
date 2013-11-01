#include <boost/thread/mutex.hpp>

#include "filtercontext.h"

namespace av {

using namespace std;


FilterContext::FilterContext()
    : ctx(0)
{
}


FilterContext::FilterContext(const Filter &filter, const string &name)
    : ctx(0)
{
    reinitInt(filter, name);
}


FilterContext::~FilterContext()
{
    //clog << "Current reference counter: " << sharedCounter.getCount() << endl;

    if (sharedCounter.getCount() == 1)
    {
        if (!sharedCounter.isManaged())
        {
            if (ctx)
            {
                avfilter_free(ctx);
            }
        }
    }
}


void FilterContext::setManaged(bool isManaged)
{
    sharedCounter.setManaged(isManaged);
}

void FilterContext::wrap(AVFilterContext *ctx)
{
    if (sharedCounter.getCount() > 1)
    {
        clog << "Can't wrap context when usage counter more 1" << endl;
        return;
    }

    this->ctx = ctx;
}




AVFilterContext *FilterContext::getAVFilterContext()
{
    return ctx;
}

bool FilterContext::isValid() const
{
    return !!ctx;
}

int FilterContext::reinit(const Filter &filter, const string &name)
{
    return reinitInt(filter, name);
}

Filter FilterContext::getFilter() const
{
    return (ctx ? Filter(ctx->filter) : Filter());
}

string FilterContext::getName() const
{
    return (ctx ? string(ctx->name) : string());
}

FilterPad FilterContext::getInputPad(unsigned int idx) const
{
    assert(ctx);

    unsigned int count;
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    count = ctx->input_count;
#else
    count = ctx->nb_inputs;
#endif

    if (idx >= count)
        return FilterPad();

    return FilterPad(&ctx->input_pads[idx]);
}

unsigned int FilterContext::getInputsCount() const
{
    assert(ctx);

    unsigned int count = 0;
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    count = ctx->input_count;
#else
    count = ctx->nb_inputs;
#endif

    return count;
}

FilterPad FilterContext::getOutputPad(unsigned int idx) const
{
    assert(ctx);

    unsigned int count = 0;
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    count = ctx->output_count;
#else
    count = ctx->nb_outputs;
#endif

    if (idx >= count)
        return FilterPad();

    return FilterPad(&ctx->output_pads[idx]);
}

int FilterContext::initFilter(const string& args, const FilterOpaque& opaque)
{
    if (!ctx)
    {
        return -1;
    }

    if (args.empty())
        return avfilter_init_filter(ctx, 0, opaque.getOpaque());
    else
        return avfilter_init_filter(ctx, args.c_str(), opaque.getOpaque());
}

int FilterContext::configLinks()
{
    if (!ctx)
    {
        return -1;
    }

    return avfilter_config_links(ctx);
}

int FilterContext::link(unsigned srcPad, const boost::shared_ptr<FilterContext> &dstFilterCtx, unsigned dstPad)
{
    if (!ctx || !dstFilterCtx)
    {
        return -1;
    }

    int stat = avfilter_link(ctx, srcPad, dstFilterCtx->getAVFilterContext(), dstPad);
    return stat;
}

//bool FilterContext::operator ==(const FilterContext &rhs)
//{
//    return ctx == rhs.ctx;
//}


FilterContext::operator bool() const
{
    return isValid();
}


unsigned int FilterContext::getOutputsCount() const
{
    assert(ctx);

    unsigned int count = 0;
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    count = ctx->output_count;
#else
    count = ctx->nb_outputs;
#endif

    return count;
}

bool FilterContext::operator !() const
{
    return !isValid();
}

int FilterContext::reinitInt(const Filter &filter, const string &name)
{
    if (ctx)
    {
        if (sharedCounter.getCount() > 1)
        {
            clog << "Can't be reinit when counter of users more 1" << endl;
            return -1;
        }

        avfilter_free(ctx);
        ctx = 0;
    }

    // FIXME: remove const_cast when upstream will be fixed
    return avfilter_open(&ctx, const_cast<AVFilter*>(filter.getAVFilter()), name.c_str());
}

void FilterContext::makeRef(const FilterContext &other)
{
    this->sharedCounter = other.sharedCounter;
    this->ctx           = other.ctx;
}

} // namespace av
