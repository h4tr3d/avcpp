#include "filter.h"

using namespace std;

namespace av {


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
    return (m_raw ? FilterPadList(m_raw->inputs) : FilterPadList());
}

FilterPadList Filter::outputs() const
{
    return (m_raw ? FilterPadList(m_raw->outputs) : FilterPadList());
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
