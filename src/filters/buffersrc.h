#ifndef AV_BUFFERSRC_H
#define AV_BUFFERSRC_H

#include "../ffmpeg.h"
#include "../filtercontext.h"
#include "../filterbufferref.h"
#include "../rational.h"
#include "../frame.h"


namespace av {

class BufferSrcFilterContext : public FilterContext
{
public:
    BufferSrcFilterContext();
    explicit BufferSrcFilterContext(const Filter& filter, const std::string &name = std::string());
    explicit BufferSrcFilterContext(const FilterContext& baseContext);

    virtual  int reinit(const Filter &filter, const string &name);

    int      addFrame(const FramePtr& frame);
    int      addBufferRef(FilterBufferRef &ref, int flags = 0);

    unsigned getFailedRequestsCount();

    static bool isFilterValid(const Filter& filter);
};

//
// Pointers
//
typedef boost::shared_ptr<BufferSrcFilterContext> BufferSrcFilterContextPtr;
typedef boost::weak_ptr<BufferSrcFilterContext> BufferSrcFilterContextWPtr;


//
// Casting
//
template <>
inline boost::shared_ptr<BufferSrcFilterContext> filter_cast(const FilterContextPtr &ctx)
{
    if (ctx && ctx->isValid() && BufferSrcFilterContext::isFilterValid(ctx->getFilter()))
    {
        return BufferSrcFilterContextPtr(new BufferSrcFilterContext(*ctx));
    }
    return BufferSrcFilterContextPtr();
}



} // namespace av

#endif // AV_BUFFERSRC_H
