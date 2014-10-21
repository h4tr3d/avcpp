#ifndef AV_FILTERINOUT_H
#define AV_FILTERINOUT_H

#include <memory>

#include "ffmpeg.h"
#include "filtercontext.h"

#include "avutils.h"
#include "linkedlistutils.h"
#include "filtergraph.h"

namespace av {

class FilterInOut
{
public:
    FilterInOut();
    FilterInOut(AVFilterInOut *inout, const FilterGraphPtr& graph = FilterGraphPtr());
    ~FilterInOut();

    AVFilterInOut *getAVFilterInOut();

    void reset(AVFilterInOut *other = 0);

    std::string      getName()          const;
    int              getPadIndex()      const;
    FilterContextPtr getFilterContext() const;

    void setName(const std::string& name);
    void setPadIndex(int index);
    void setFilterContext(const FilterContextPtr& ctx);

    FilterGraphPtr getFilterGraph() const;
    void           setFilterGraph(const FilterGraphPtr& graph);



private:
    AVFilterInOut    *inout;
    FilterGraphPtr    graph;
};

struct FilterInOutRawCast
{
    AVFilterInOut* operator ()(FilterInOut& w)
    {
        return w.getAVFilterInOut();
    }
};

typedef DefaultResetPtr<AVFilterInOut, FilterInOut> FilterInOutResetPtr;

class FilterInOutList :
        public LinkedListWrapper<AVFilterInOut, FilterInOut, AvNextElement, FilterInOutRawCast, FilterInOutResetPtr, AvDeleter>,
        private noncopyable
{
private:
    FilterGraphPtr graph;


public:
    using LinkedListWrapper::beginPtr;
    using LinkedListWrapper::endPtr;

    FilterInOutList() : LinkedListWrapper()
    {}

    explicit FilterInOutList(AVFilterInOut * ptr, const FilterGraphPtr& graph)
        : LinkedListWrapper(ptr),
          graph(graph)
    {}

    virtual iterator begin()
    {
        iterator it(beginPtr);
        it.wrapper.setFilterGraph(graph);
        return it;
    }

    virtual const_iterator begin() const
    {
        const_iterator it(beginPtr);
        it.wrapper.setFilterGraph(graph);
        return it;
    }


    bool alloc()
    {
        if (isValid())
            return false;

        beginPtr = avfilter_inout_alloc();
        endPtr = beginPtr;
        beginPtr->next = 0;

        return isValid();
    }
};

typedef std::shared_ptr<FilterInOutList> FilterInOutListPtr;
typedef std::weak_ptr<FilterInOutList> FilterInOutListWPtr;


} // namespace av

#endif // AV_FILTERINOUT_H
