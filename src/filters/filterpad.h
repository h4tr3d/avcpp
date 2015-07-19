#ifndef AV_FILTERPAD_H
#define AV_FILTERPAD_H

#include <iterator>
#include <memory>
#include <type_traits>

#include "ffmpeg.h"
#include "avutils.h"
#include "linkedlistutils.h"

namespace av {

/**
 * Describe one Filter Pad
 */
template<typename T>
class FilterPadBase : public FFWrapperPtr<T>
{
public:
    static_assert(std::is_same<typename std::remove_const<T>::type, AVFilterPad>::value,
                  "This template only valid for const and non-const AVFilterPad");

    // This template only valid for const and non-const AVFilterPad
    using BaseType = FFWrapperPtr<T>;

    // Import ctors
    using BaseType::FFWrapperPtr;

    std::string name() const
    {
        const auto nm = nameCStr();
        return nm ? nm : std::string();
    }

    const char* nameCStr() const noexcept
    {
        return (m_raw ? avfilter_pad_get_name(m_raw, 0) : nullptr);
    }

    AVMediaType mediaType() const
    {
        return (m_raw ? avfilter_pad_get_type(m_raw, 0) : AVMEDIA_TYPE_UNKNOWN);
    }

protected:
    using BaseType::m_raw;
};

/// Pads, including dynamic one at the FilterContext
using FilterPad       = FilterPadBase<AVFilterPad>;
/// Pads, only static, declared at the Filter
using FilterPadStatic = FilterPadBase<const AVFilterPad>;

///
///

struct FilterPadRawCast
{
    const AVFilterPad* operator ()(FilterPadStatic& w)
    {
        return w.raw();
    }
};

typedef DefaultResetPtr<const AVFilterPad, FilterPadStatic> FilterPadResetPtr;

class FilterPadList :
        public LinkedListWrapper<const AVFilterPad, FilterPadStatic, AvNextElement, FilterPadRawCast, FilterPadResetPtr>
{
public:
    using LinkedListWrapper::m_begin;
    using LinkedListWrapper::m_end;

    FilterPadList() : LinkedListWrapper() {}
    FilterPadList(const AVFilterPad * ptr) : LinkedListWrapper(ptr) {}

    size_t count() const;

private:
    mutable size_t m_countCached = 0;
};


} // namespace av

#endif // AV_FILTERPAD_H
