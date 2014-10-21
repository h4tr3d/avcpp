#include "filter.h"

namespace av {

Filter::Filter()
{
    init();
}

Filter::Filter(const std::string &name)
{
    init();
    setFilter(name);
}

Filter::Filter(const AVFilter *filter)
{
    init();
    this->filter = filter;
}

Filter::Filter(const Filter &other)
{
    init();
    this->filter = other.filter;
}

Filter::~Filter()
{
}

bool Filter::isValid() const
{
    return !!filter;
}

bool Filter::setFilter(const std::string& name)
{
    filter = avfilter_get_by_name(name.c_str());
    return !!filter;
}

void Filter::setFilter(const AVFilter *filter)
{
    this->filter = filter;
}

std::string Filter::getName() const
{
    return (filter ? std::string(filter->name) : std::string());
}

std::string Filter::getDescription() const
{
    return (filter ? std::string(filter->description) : std::string());
}

FilterPadList Filter::getInputs() const
{
    return (filter ? FilterPadList(filter->inputs) : FilterPadList());
}

FilterPadList Filter::getOutputs() const
{
    return (filter ? FilterPadList(filter->outputs) : FilterPadList());
}

const AVFilter *Filter::getAVFilter() const
{
    return filter;
}

void Filter::init()
{
    filter = 0;
}

} // namespace av
