#include "filterpad.h"

using namespace std;

namespace av {

size_t FilterPadList::count() const
{
    if (m_countCached)
        return m_countCached;
    return m_begin ? (m_countCached = avfilter_pad_count(m_begin)) : 0;
}


} // namespace av
