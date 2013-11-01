#include "buffersrc.h"

using namespace std;

namespace av {

BufferSrcFilterContext::BufferSrcFilterContext()
{
}

BufferSrcFilterContext::BufferSrcFilterContext(const Filter &filter, const string &name)
{
    if (isFilterValid(filter))
    {
        reinitInt(filter, name);
    }
}

BufferSrcFilterContext::BufferSrcFilterContext(const FilterContext &baseContext)
{
    if (baseContext && isFilterValid(baseContext.getFilter()))
    {
        makeRef(baseContext);
    }
}

int BufferSrcFilterContext::reinit(const Filter &filter, const string &name)
{
    if (isFilterValid(filter))
    {
        return reinitInt(filter, name);
    }

    clog << "You pass invalid filter for context: " << (filter.isValid() ? filter.getName() : "(null)") << endl;

    return -1;
}

int BufferSrcFilterContext::addFrame(const FramePtr &frame)
{
    assert(isValid());
    assert(frame);

    return av_buffersrc_add_frame(getAVFilterContext(), frame->getAVFrame());
}

int BufferSrcFilterContext::addBufferRef(FilterBufferRef &ref, int flags)
{
    assert(isValid());
    assert(ref.isValid());

    return av_buffersrc_add_ref(getAVFilterContext(), ref.getAVFilterBufferRef(), flags);
}

unsigned BufferSrcFilterContext::getFailedRequestsCount()
{
    assert(isValid());

    return av_buffersrc_get_nb_failed_requests(getAVFilterContext());
}

bool BufferSrcFilterContext::isFilterValid(const Filter &filter)
{
    if (filter.isValid())
    {
        if (filter.getName() == "buffer" ||
            filter.getName() == "abuffer")
        {
            return true;
        }
    }
    return false;
}

} // namespace av
