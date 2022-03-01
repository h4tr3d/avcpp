#include "filterpad.h"

using namespace std;

namespace av {

FilterPadList::FilterPadList(const AVFilterPad *begin, size_t count)
    : FFWrapperPtr<const AVFilterPad>(begin),
      m_count(count)
{
}

size_t FilterPadList::count() const noexcept
{
    return m_count;
}

string FilterPadList::name(size_t index) const noexcept
{
    return avfilter_pad_get_name(m_raw, static_cast<int>(index));
}

const char *FilterPadList::nameCStr(size_t index) const noexcept
{
    return avfilter_pad_get_name(m_raw, static_cast<int>(index));
}

AVMediaType FilterPadList::type(size_t index) const noexcept
{
    return avfilter_pad_get_type(m_raw, static_cast<int>(index));
}

} // namespace av
