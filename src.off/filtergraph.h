#ifndef AV_FILTERGRAPH_H
#define AV_FILTERGRAPH_H

#include <map>
#include <memory>

#include "ffmpeg.h"
#include "container.h"
#include "streamcoder.h"

#include "filtercontext.h"
#include "filteropaque.h"

#include "filters/buffersink.h"
#include "filters/buffersrc.h"

namespace av {

class FilterInOutList;
typedef std::shared_ptr<FilterInOutList> FilterInOutListPtr;

class FilterGraph : public std::enable_shared_from_this<FilterGraph>
{
    friend class FilterInOut;

public:
    FilterGraph();
    ~FilterGraph();

    // Context getters/setters
    bool        isValid();
    void        setScaleSwsOptions(const std::string& opts);
    std::string getScaleSwsOptions() const;
    int         getFiltersCount() const;
    void        setAutoConvert(unsigned flags);

    // Public API

    FilterContextPtr getFilter(const std::string& name);
    FilterContextPtr getFilter(unsigned idx);
    FilterContextPtr addFilter(const Filter &filter, const std::string &name);
    int              createFilter(const Filter &filter,
                                  const std::string& filterName,
                                  const std::string& filterArgs = std::string(),
                                  const FilterOpaque& opaque = FilterOpaque());

    int              parse(const std::string&      graphDescription,
                           const FilterContextPtr& srcFilterCtx,
                           const FilterContextPtr& sinkFilterCtx);

    int              parse(const std::string&  graphDescription,
                           FilterInOutListPtr& inputs,
                           FilterInOutListPtr& outputs);

    int              config();

    std::string      dump(bool doPrint = true, const std::string& options = std::string());


    BufferSrcFilterContextPtr  getSrcFilter()  const;
    BufferSinkFilterContextPtr getSinkFilter() const;

    void setSrcFilter(const BufferSrcFilterContextPtr &filterCtx);
    void setSinkFilter(const BufferSinkFilterContextPtr &filterCtx);

    static std::shared_ptr<FilterGraph> createSimpleAudioFilterGraph(
            const Rational&             srcTimeBase,
            int                         srcSampleRate,
            AVSampleFormat              srcSampleFormat,
            uint64_t                    srcChannelLayout,
            const list<int>&            dstSampleRates,
            const list<AVSampleFormat>& dstSampleFormats,
            const list<uint64_t>&       dstChannelLayouts,
            const std::string&          graphDescription);

    static std::shared_ptr<FilterGraph> createSimpleVideoFilterGraph(
            const Rational&    timeBase,
            const Rational&    sampleAspectRatio,
            const Rational&    frameRate,
            int                srcWidth,
            int                srcHeight,
            PixelFormat        srcPixelFormat,
            int                dstWidth,
            int                dstHeight,
            PixelFormat        dstPixelFormat,
            const std::string& graphDescription,
            int                swsFlags = SWS_BILINEAR);

private:
    void addManagedWrapper(AVFilterContext *ctx);
    FilterContextPtr getFilter(AVFilterContext *ctx);

private:
    AVFilterGraph *m_graph;

    std::map<AVFilterContext *, FilterContextPtr> filtersMapping;

    BufferSrcFilterContextPtr  srcFilterContext;
    BufferSinkFilterContextPtr sinkFilterContext;
};

typedef std::shared_ptr<FilterGraph> FilterGraphPtr;
typedef std::weak_ptr<FilterGraph>   FilterGraphWPtr;

} // namespace av

#endif // AV_FILTERGRAPH_H
