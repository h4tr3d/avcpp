#ifndef AV_FILTERCONTEXT_H
#define AV_FILTERCONTEXT_H

#include <memory>

#include "ffmpeg.h"
#include "filter.h"
#include "filteropaque.h"
#include "filtercontext_helper.h"

namespace av {

class FilterContext
{
    friend class FilterGraph;

public:
    FilterContext();
    explicit FilterContext(const Filter& filter, const std::string &name);
    virtual ~FilterContext();

    AVFilterContext* getAVFilterContext();    

    bool isValid() const;

    virtual int  reinit(const Filter& filter, const std::string &name);

    Filter            getFilter() const;
    std::string       getName() const;

    unsigned int      getInputsCount()  const;
    FilterPad         getInputPad(unsigned int idx)    const;

    unsigned int      getOutputsCount() const;
    FilterPad         getOutputPad(unsigned int idx)   const;

    int  initFilter(const std::string &args, const FilterOpaque& opaque = FilterOpaque());
    int  configLinks();
    int  link(unsigned srcPad, const std::shared_ptr<FilterContext>& dstFilter, unsigned dstPad);

    // TODO:
    // int insertFilter()

    operator bool() const;
    bool operator!() const;

protected:
    int reinitInt(const Filter& filter, const std::string& name);
    void makeRef(const FilterContext& other);

private:

    // Used by FilterGraph: create AVFilterGraph manager filter
    // So we don't need free filter - its will be completed by avfilter_graph_free() automaticaly
    void setManaged(bool isManaged);
    void wrap(AVFilterContext *ctx);

private:
    AVFilterContext                    *ctx;
    detail::FilterContextSharedCounter  sharedCounter;
};

typedef std::shared_ptr<FilterContext> FilterContextPtr;
typedef std::weak_ptr<FilterContext> FilterContextWPtr;


template <typename T>
inline std::shared_ptr<T> filter_cast(const FilterContextPtr&)
{
    return std::shared_ptr<T>();
}


} // namespace av

#endif // AV_FILTERCONTEXT_H
