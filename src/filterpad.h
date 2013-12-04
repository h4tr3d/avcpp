#ifndef AV_FILTERPAD_H
#define AV_FILTERPAD_H

#include <iterator>
#include <memory>

#include "ffmpeg.h"
#include "avutils.h"
#include "linkedlistutils.h"

namespace av {

/**
 * Describe one Filter Pad
 */
class FilterPad
{
public:
    FilterPad();
    FilterPad(const AVFilterPad *pad);
    FilterPad(const FilterPad& other);
    virtual ~FilterPad();

    const AVFilterPad* getAVFilterPad() const;
    void reset(const AVFilterPad *pad = 0);

    bool               isValid()        const;

    const char*        getName()        const;
    AVMediaType        getMediaType()   const;

private:
    const AVFilterPad *pad;
};


///
///

struct FilterPadRawCast
{
    const AVFilterPad* operator ()(FilterPad& w)
    {
        return w.getAVFilterPad();
    }
};

typedef DefaultResetPtr<const AVFilterPad, FilterPad> FilterPadResetPtr;

class FilterPadList :
        public LinkedListWrapper<const AVFilterPad, FilterPad, AvNextElement, FilterPadRawCast, FilterPadResetPtr>
{
public:
    using LinkedListWrapper::beginPtr;
    using LinkedListWrapper::endPtr;

    FilterPadList() : LinkedListWrapper() {}
    FilterPadList(const AVFilterPad * ptr) : LinkedListWrapper(ptr) {}

};


} // namespace av

#endif // AV_FILTERPAD_H
