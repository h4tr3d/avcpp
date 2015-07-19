#include <mutex>
#include <cassert>

#include "filtercontext.h"

namespace av {

using namespace std;

FilterContext::FilterContext(AVFilterContext *ctx)
    : FFWrapperPtr<AVFilterContext>(ctx)
{
}

Filter FilterContext::filter() const
{
    return (m_raw ? Filter(m_raw->filter) : Filter());
}

string FilterContext::name() const
{
    if (m_raw) {
        if (m_raw->name) {
            return m_raw->name;
        } else if (m_raw->filter->name) {
            return m_raw->filter->name;
        }
    }
    return string();
}

FilterPad FilterContext::inputPad(size_t idx) const
{
    assert(m_raw);

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    auto count = m_raw->input_count;
#else
    auto count = m_raw->nb_inputs;
#endif

    if (idx >= count)
        return FilterPad();

    return FilterPad(&m_raw->input_pads[idx]);
}

size_t FilterContext::inputsCount() const
{
    assert(m_raw);

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    auto count = m_raw->input_count;
#else
    auto count = m_raw->nb_inputs;
#endif

    return count;
}

FilterPad FilterContext::outputPad(size_t idx) const
{
    assert(m_raw);

    unsigned int count = 0;
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    count = m_raw->output_count;
#else
    count = m_raw->nb_outputs;
#endif

    if (idx >= count)
        return FilterPad();

    return FilterPad(&m_raw->output_pads[idx]);
}

void FilterContext::init(const string& args, error_code &ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    const char* cargs = args.empty() ? nullptr : args.c_str();
    int sts = avfilter_init_str(m_raw, cargs);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
    }
}

void FilterContext::free()
{
    avfilter_free(m_raw);
}

void FilterContext::link(unsigned srcPad, FilterContext &dstFilter, unsigned dstPad, error_code &ec)
{
    clear_if(ec);
    if (!m_raw || !dstFilter) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    int sts = avfilter_link(m_raw, srcPad, dstFilter.raw(), dstPad);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }
}


FilterContext::operator bool() const
{
    return !isNull();
}


size_t FilterContext::outputsCount() const
{
    assert(m_raw);

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    auto count = m_raw->output_count;
#else
    auto count = m_raw->nb_outputs;
#endif

    return count;
}

} // namespace av
