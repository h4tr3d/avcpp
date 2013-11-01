#include "filterpad.h"

namespace av {

FilterPad::FilterPad()
    : pad(0)
{
}

FilterPad::FilterPad(const AVFilterPad *pad)
    : pad(pad)
{
}

FilterPad::FilterPad(const FilterPad &other)
    : pad(other.pad)
{
}

FilterPad::~FilterPad()
{
}

const AVFilterPad *FilterPad::getAVFilterPad() const
{
    return pad;
}

void FilterPad::reset(const AVFilterPad *pad)
{
    this->pad = pad;
}

bool FilterPad::isValid() const
{
    return !!pad;
}

const char *FilterPad::getName() const
{
    return (pad ? avfilter_pad_get_name(const_cast<AVFilterPad*>(pad), 0) : 0);
}

AVMediaType FilterPad::getMediaType() const
{
    return (pad ? avfilter_pad_get_type(const_cast<AVFilterPad*>(pad), 0) : AVMEDIA_TYPE_UNKNOWN);
}


} // namespace av
