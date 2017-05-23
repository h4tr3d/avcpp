#ifndef AV_FILTERCONTEXT_H
#define AV_FILTERCONTEXT_H

#include <memory>

#include "ffmpeg.h"
#include "filter.h"
#include "averror.h"
//#include "filteropaque.h"
//#include "filtercontext_helper.h"

namespace av {

class FilterContext : public FFWrapperPtr<AVFilterContext>
{
    friend class FilterGraph;

public:
    using FFWrapperPtr<AVFilterContext>::FFWrapperPtr;

    FilterContext(AVFilterContext *ctx);
    FilterContext() = default;

    Filter            filter() const;
    std::string       name() const;

    size_t            inputsCount()  const;
    size_t            outputsCount() const;

    void init(const std::string &args, OptionalErrorCode ec = throws());
    void free();

    void link(unsigned srcPad, FilterContext& dstFilter, unsigned dstPad, OptionalErrorCode ec = throws());

    operator bool() const;

protected:

};


} // namespace av

#endif // AV_FILTERCONTEXT_H
