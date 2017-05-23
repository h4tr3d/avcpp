#ifndef AV_FILTERGRAPH_H
#define AV_FILTERGRAPH_H

#include <map>
#include <memory>

#include "ffmpeg.h"
#include "filtercontext.h"
#include "buffersink.h"
#include "buffersrc.h"

#include "averror.h"

namespace av {

class FilterGraph : FFWrapperPtr<AVFilterGraph>
{
    friend class FilterInOut;

public:
    FilterGraph();
    FilterGraph(decltype(nullptr));
    ~FilterGraph();

    FilterGraph(const FilterGraph&) = delete;
    void operator=(const FilterGraph&) = delete;

    // Context getters/setters
    bool        isValid();
    //void        setScaleSwsOptions(const std::string& opts);
    int         filtersCount() const;
    void        setAutoConvert(unsigned flags);

    // Public API

    FilterContext filter(const std::string& name, OptionalErrorCode ec = throws());
    FilterContext filter(unsigned idx, OptionalErrorCode ec = throws());

    FilterContext allocFilter(const Filter &filter, const std::string &name, OptionalErrorCode ec = throws());
    FilterContext createFilter(const Filter &filter,
                               const std::string& filterName,
                               const std::string& filterArgs,
                               OptionalErrorCode ec = throws());

    void parse(const std::string &graphDescription,
               FilterContext     &srcFilterCtx,
               FilterContext     &sinkFilterCtx,
               OptionalErrorCode  ec = throws());

    void config(OptionalErrorCode ec = throws());

    std::string dump(bool doPrint = true, const std::string& options = std::string());

    BufferSrcFilterContext  bufferSrcFilter(OptionalErrorCode ec = throws());
    BufferSinkFilterContext bufferSinkFilter(OptionalErrorCode ec = throws());


private:

private:

    BufferSrcFilterContext  m_bufferSrc;
    bool                    m_bufferSrcSearchDone = false;
    BufferSinkFilterContext m_bufferSink;
    bool                    m_bufferSinkSearchDone = false;

    bool m_configured = false;
};


} // namespace av

#endif // AV_FILTERGRAPH_H
