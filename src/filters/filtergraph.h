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

    FilterContext filter(const std::string& name, std::error_code &ec = throws());
    FilterContext filter(unsigned idx, std::error_code &ec = throws());

    FilterContext allocFilter(const Filter &filter, const std::string &name, std::error_code &ec = throws());
    FilterContext createFilter(const Filter &filter,
                               const std::string& filterName,
                               const std::string& filterArgs,
                               std::error_code &ec = throws());

    void parse(const std::string &graphDescription,
               FilterContext     &srcFilterCtx,
               FilterContext     &sinkFilterCtx,
               std::error_code   &ec = throws());

    void config(std::error_code &ec = throws());

    std::string dump(bool doPrint = true, const std::string& options = std::string());

    BufferSrcFilterContext  bufferSrcFilter(std::error_code &ec = throws());
    BufferSinkFilterContext bufferSinkFilter(std::error_code &ec = throws());

#if 0
    BufferSrcFilterContextPtr  getSrcFilter()  const;
    BufferSinkFilterContextPtr getSinkFilter() const;

    void setSrcFilter(const BufferSrcFilterContextPtr &filterCtx);
    void setSinkFilter(const BufferSinkFilterContextPtr &filterCtx);

    static std::shared_ptr<FilterGraph> createSimpleAudioFilterGraph(
            const Rational&             srcTimeBase,
            int                         srcSampleRate,
            AVSampleFormat              srcSampleFormat,
            uint64_t                    srcChannelLayout,
            const std::list<int>&            dstSampleRates,
            const std::list<AVSampleFormat>& dstSampleFormats,
            const std::list<uint64_t>&       dstChannelLayouts,
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
#endif

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
