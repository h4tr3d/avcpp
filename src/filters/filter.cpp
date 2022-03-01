#include "filter.h"

using namespace std;

namespace av {

static size_t get_pad_count(const AVFilter *filter, bool output)
{
#if LIBAVFILTER_VERSION_MAJOR >= 8 // FFmpeg 5.0
    return avfilter_filter_pad_count(filter, output);
#else
    return avfilter_pad_count(output ? filter->outputs : filter->inputs);
#endif
}

Filter::Filter(const AVFilter *ptr)
    : FFWrapperPtr<const AVFilter>(ptr)
{
}

Filter::Filter(const std::string &name)
{
    setFilter(name);
}

Filter::Filter(const char *name)
{
    setFilter(name);
}

bool Filter::setFilter(const std::string& name)
{
    return setFilter(name.c_str());
}

bool Filter::setFilter(const char *name)
{
    m_raw = avfilter_get_by_name(name);
    return m_raw;
}

std::string Filter::name() const
{
    return RAW_GET(name, string());
}

string Filter::description() const
{
    return RAW_GET(description, string());
}

FilterPadList Filter::inputs() const
{
    return (m_raw ? FilterPadList(m_raw->inputs, get_pad_count(m_raw, false)) : FilterPadList());
}

FilterPadList Filter::outputs() const
{
    return (m_raw ? FilterPadList(m_raw->outputs, get_pad_count(m_raw, true)) : FilterPadList());
}

int Filter::flags() const
{
    return RAW_GET(flags, 0);
}

av::Filter::operator bool() const
{
    return !isNull();
}

} // namespace av
