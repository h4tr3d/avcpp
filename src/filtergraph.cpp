#define __STDC_FORMAT_MACROS 1

#include <list>
#include <sstream>
#include <cassert>

#include "avutils.h"
#include "filtergraph.h"
#include "filterinout.h"

using namespace std;

namespace av {




FilterGraph::FilterGraph()
{
    m_graph = avfilter_graph_alloc();
}

FilterGraph::~FilterGraph()
{
    avfilter_graph_free(&m_graph);
}

bool FilterGraph::isValid()
{
    return !!m_graph;
}

void FilterGraph::setScaleSwsOptions(const string &opts)
{
    if (m_graph)
    {
        if (m_graph->scale_sws_opts)
        {
             av_freep(m_graph->scale_sws_opts);
             m_graph->scale_sws_opts = 0;
        }

        m_graph->scale_sws_opts = av_strdup(opts.c_str());
    }
}

string FilterGraph::getScaleSwsOptions() const
{
    return (m_graph ? string(m_graph->scale_sws_opts) : string());
}

int FilterGraph::getFiltersCount() const
{
    return (m_graph ? m_graph->nb_filters : 0);
}

void FilterGraph::setAutoConvert(unsigned flags)
{
    if (m_graph)
    {
        avfilter_graph_set_auto_convert(m_graph, flags);
    }
}


FilterContextPtr FilterGraph::getFilter(const string &name)
{
    if (!m_graph)
        return FilterContextPtr();

    AVFilterContext *ctx = avfilter_graph_get_filter(m_graph, const_cast<char*>(name.c_str()));

    if (!ctx)
        return FilterContextPtr();

    addManagedWrapper(ctx);

    return filtersMapping[ctx];
}

FilterContextPtr FilterGraph::getFilter(unsigned idx)
{
    if (!m_graph)
        return FilterContextPtr();

    assert(idx < m_graph->nb_filters);

    AVFilterContext *ctx = m_graph->filters[idx];

    if (!ctx)
        return FilterContextPtr();

    addManagedWrapper(ctx);

    return filtersMapping[ctx];

}

FilterContextPtr FilterGraph::addFilter(const Filter &filter, const std::string &name)
{
    assert(m_graph);
    
    AVFilterContext *ctx = avfilter_graph_alloc_filter(m_graph, filter.getAVFilter(), name.c_str());
    if (ctx)
    {
        addManagedWrapper(ctx);
        return filtersMapping[ctx];
    }
    
    return FilterContextPtr();
}

int FilterGraph::createFilter(const Filter &filter, const string &filterName, const string &filterArgs, const FilterOpaque &opaque)
{
    if (!m_graph || !filter.isValid())
        return AVERROR(AVERROR_UNKNOWN);

    AVFilterContext *ctx = nullptr;

    int stat = avfilter_graph_create_filter(&ctx,
                                            filter.getAVFilter(),
                                            filterName.c_str(),
                                            filterArgs.empty() ? nullptr : filterArgs.c_str(),
                                            opaque.getOpaque(),
                                            m_graph);

    clog << "create filter: " << filterName << ", stat: " << stat << ", " << error2string(stat) << endl;

    if (stat >= 0 && ctx)
    {
        addManagedWrapper(ctx);
    }

    return stat;
}

int FilterGraph::parse(const string &graphDescription,
                       const FilterContextPtr &srcFilterCtx,
                       const FilterContextPtr &sinkFilterCtx)
{
    if (!m_graph || !srcFilterCtx || !sinkFilterCtx)
        return -1;

    if (filtersMapping.find(srcFilterCtx->getAVFilterContext()) == filtersMapping.end())
    {
        clog << "Source filter does not present in filter graph" << endl;
        return -1;
    }

    if (filtersMapping.find(sinkFilterCtx->getAVFilterContext()) == filtersMapping.end())
    {
        clog << "Sink filter does not present in filter graph" << endl;
        return -1;
    }

    // TODO make scoped free, or use FilterInOut instead
    AVFilterInOut *outputs = 0;
    AVFilterInOut *inputs  = 0;

    if (graphDescription.empty())
    {
        int stat = srcFilterCtx->link(0, sinkFilterCtx, 0);
        return stat;
    }
    else
    {
        outputs = avfilter_inout_alloc();
        inputs  = avfilter_inout_alloc();

        if (!outputs || !inputs)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            return AVERROR(ENOMEM);
        }

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = srcFilterCtx->getAVFilterContext();
        outputs->pad_idx    = 0;
        outputs->next       = 0;

        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = sinkFilterCtx->getAVFilterContext();
        inputs->pad_idx     = 0;
        inputs->next        = 0;

        int stat = avfilter_graph_parse(m_graph, graphDescription.c_str(), &inputs, &outputs, 0);
        //if (stat < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
        }
        return stat;
    }
}

int FilterGraph::parse(const string &graphDescription,
                       FilterInOutListPtr &inputs,
                       FilterInOutListPtr &outputs)
{
    if (!m_graph || graphDescription.empty())
        return -1;

    AVFilterInOut *inputsRaw = 0;
    AVFilterInOut *outputsRaw = 0;
    int stat;

    if (inputs)
        inputsRaw = inputs->raw();
    if (outputs)
        outputsRaw = outputs->raw();

    // TODO add preprocessor directive to detect correct version where avfilter_graph_parse2() is present
    if (inputsRaw || outputsRaw)
    {
        stat = avfilter_graph_parse(m_graph, graphDescription.c_str(), &inputsRaw, &outputsRaw, 0);
    }
    else
    {
        stat = avfilter_graph_parse2(m_graph, graphDescription.c_str(), &inputsRaw, &outputsRaw);
    }

    inputs = FilterInOutListPtr();
    outputs = FilterInOutListPtr();

    if (stat >= 0)
    {
        if (inputsRaw)
            inputs = FilterInOutListPtr(new FilterInOutList(inputsRaw, shared_from_this()));

        if (outputsRaw)
            outputs = FilterInOutListPtr(new FilterInOutList(outputsRaw, shared_from_this()));
    }

    return stat;
}

int FilterGraph::config()
{
    if (!m_graph)
        return -1;

    int stat = avfilter_graph_config(m_graph, 0);
    return stat;
}

string FilterGraph::dump(bool doPrint, const string &options)
{
    string result;

    if (m_graph)
    {
        result = avfilter_graph_dump(m_graph, options.c_str());
        if (doPrint)
            clog << result;
    }

    return result;
}

BufferSrcFilterContextPtr FilterGraph::getSrcFilter() const
{
    return srcFilterContext;
}

BufferSinkFilterContextPtr FilterGraph::getSinkFilter() const
{
    return sinkFilterContext;
}

std::shared_ptr<FilterGraph> FilterGraph::createSimpleAudioFilterGraph(
        // Inputs
        const Rational             &srcTimeBase,
        int                         srcSampleRate,
        AVSampleFormat              srcSampleFormat,
        uint64_t                    srcChannelLayout,
        // Outputs
        const list<int>            &dstSampleRates,
        const list<AVSampleFormat> &dstSampleFormats,
        const list<uint64_t>       &dstChannelLayouts,
        // Common
        const string               &graphDescription)
{
    FilterGraphPtr graph(new FilterGraph());

    const string srcFilterName     = "_avcpp main audio input";
    const string sinkFilterName    = "_avcpp main audio sink";
    const string aformatFilterName = "_avcpp audio format for output stream";

    //
    // Src filter
    //
    Filter           srcFilter("abuffer");
    BufferSrcFilterContextPtr srcFilterCtx;

    if (!srcFilter.isValid())
    {
        clog << "Can't found src filter: abuffer" << endl;
        return FilterGraphPtr();
    }

    stringstream ss;
    ss << "time_base=" << dec << srcTimeBase << ":sample_rate=" << srcSampleRate <<
          ":sample_fmt=" << av_get_sample_fmt_name(srcSampleFormat) <<
          ":channel_layout=0x" << hex << srcChannelLayout;

    string abufferArgs = ss.str();

    clog << "abuffer args: " << abufferArgs << endl;

    if (graph->createFilter(srcFilter, srcFilterName, abufferArgs) < 0)
    {
        clog << "Can't create filter 'abuffer' in filter graph" << endl;
        return FilterGraphPtr();
    }

    srcFilterCtx = filter_cast<BufferSrcFilterContext>(graph->getFilter(srcFilterName));

    //
    // Sink filter
    //
    Filter           sinkFilter("ffabuffersink");
    //FilterContextPtr sinkFilterCtx;
    BufferSinkFilterContextPtr sinkFilterCtx;
    if (!sinkFilter.isValid())
    {
        clog << "Can't found sink filter: ffabuffersink" << endl;
        return FilterGraphPtr();
    }

    if (graph->createFilter(sinkFilter, sinkFilterName) < 0)
    {
        clog << "Can't create filter 'ffabuffersink' in filter graph" << endl;
        return FilterGraphPtr();
    }

    sinkFilterCtx = filter_cast<BufferSinkFilterContext>(graph->getFilter(sinkFilterName));


    //
    // Output aformat filter
    //
    FilterContextPtr aformatFilterCtx;
    if (!dstSampleRates.empty() || !dstSampleFormats.empty() || !dstChannelLayouts.empty())
    {
        string aformatArgs;

        Filter aformatFilter("aformat");
        if (!aformatFilter.isValid())
        {
            clog << "Can't found aformat filter" << endl;
            return FilterGraphPtr();
        }

        if (!dstSampleRates.empty())
        {
            string sampleRates = "sample_rates=";
            for (list<int>::const_iterator it = dstSampleRates.begin();
                 it != dstSampleRates.end();
                 ++it)
            {
                string tmp;

                if (it != dstSampleRates.begin())
                    tmp = ",";

                stringstream ss;
                ss << dec << *it;

                tmp += ss.str();
                sampleRates += tmp;
            }

            aformatArgs += sampleRates + ":";
        }

        if (!dstSampleFormats.empty())
        {
            string sampleFormats = "sample_fmts=";
            for (list<AVSampleFormat>::const_iterator it = dstSampleFormats.begin();
                 it != dstSampleFormats.end();
                 ++it)
            {
                string tmp;

                if (it != dstSampleFormats.begin())
                    tmp = ",";

                tmp += string(av_get_sample_fmt_name(*it));

                //tmp += boost::str(boost::format("%s") % av_get_sample_fmt_name(*it));
                sampleFormats += tmp;
            }

            aformatArgs += sampleFormats + ":";
        }

        if (!dstChannelLayouts.empty())
        {
            string channelLayouts = "channel_layouts=";
            for (list<uint64_t>::const_iterator it = dstChannelLayouts.begin();
                 it != dstChannelLayouts.end();
                 ++it)
            {
                string tmp;

                if (it != dstChannelLayouts.begin())
                    tmp = ",";

                stringstream ss;
                ss << "0x" << hex << *it;

                tmp += ss.str();

                //tmp += boost::str(boost::format("0x%"PRIx64) % *it);
                channelLayouts += tmp;
            }

            aformatArgs += channelLayouts + ":";
        }

        clog << "aformat args: " << aformatArgs << endl;

        if (graph->createFilter(aformatFilter, aformatFilterName, aformatArgs) < 0)
        {
            clog << "Can't create filter aformat in filter graph" << endl;
            return FilterGraphPtr();
        }

        aformatFilterCtx = graph->getFilter(aformatFilterName);
    }

    //
    // Link filters
    //
    FilterContextPtr srcTmp = srcFilterCtx;
    if (aformatFilterCtx)
    {
        if (srcFilterCtx->link(0, aformatFilterCtx, 0) < 0)
        {
            clog << "Can't link src filter to aformat filter" << endl;
            return FilterGraphPtr();
        }

        srcTmp = aformatFilterCtx;
    }

    if (graph->parse(graphDescription, srcTmp, sinkFilterCtx) < 0)
    {
        clog << "Can't parse graph description: " << graphDescription << endl;
        return FilterGraphPtr();
    }

    if (graph->config() < 0)
    {
        clog << "Can't configure graph" << endl;
        return FilterGraphPtr();
    }

    graph->setSrcFilter(srcFilterCtx);
    graph->setSinkFilter(sinkFilterCtx);

    return graph;
}

std::shared_ptr<FilterGraph> FilterGraph::createSimpleVideoFilterGraph(
        // Common
        const Rational &timeBase,
        const Rational &sampleAspectRatio,
        const Rational &frameRate,
        // Src
        int srcWidth,
        int srcHeight,
        PixelFormat srcPixelFormat,
        // Dst
        int dstWidth,
        int dstHeight,
        PixelFormat dstPixelFormat,
        // Graph description
        const string &graphDescription,
        int   swsFlags)
{
    FilterGraphPtr graph(new FilterGraph());

    const string srcFilterName    = "_avcpp main video input";
    const string sinkFilterName   = "_avcpp main video sink";
    const string scaleFilterName  = "_avcpp video scale for output stream";
    const string formatFilterName = "_avcpp video format for output stream";


    //
    // Src filter
    //
    Filter srcFilter("buffer");
    BufferSrcFilterContextPtr srcFilterCtx;

    if (!srcFilter.isValid())
    {
        clog << "Can't found src filter: buffer" << endl;
        return FilterGraphPtr();
    }

    stringstream ss;
    ss << "video_size=" << dec << srcWidth << "x" << srcHeight <<
          ":pix_fmt=" << srcPixelFormat <<
          ":time_base=" << timeBase <<
          ":pixel_aspect=" << sampleAspectRatio <<
          ":sws_param=flags=" << swsFlags;

    string bufferArgs = ss.str();
#if 0
            boost::str(boost::format("video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:"
                                     "pixel_aspect=%d/%d:sws_param=flags=%d")
                       % srcWidth
                       % srcHeight
                       % srcPixelFormat
                       % timeBase.getNumerator()
                       % timeBase.getDenominator()
                       % sampleAspectRatio.getNumerator()
                       % sampleAspectRatio.getDenominator()
                       % swsFlags
                       );
#endif

    if (frameRate.getDenominator() && frameRate.getNumerator())
    {
        ss.flush();
        ss << ":frame_rate=" << frameRate;
        bufferArgs += ss.str();
                //boost::str(boost::format(":frame_rate=%d/%d") % frameRate.getNumerator() % frameRate.getDenominator());
    }

    clog << "buffer args: " << bufferArgs << endl;

    if (graph->createFilter(srcFilter, srcFilterName, bufferArgs) < 0)
    {
        clog << "Can't create filter 'buffer' in filter graph" << endl;
        return FilterGraphPtr();
    }

    srcFilterCtx = filter_cast<BufferSrcFilterContext>(graph->getFilter(srcFilterName));


    //
    // Sink filter
    //
    Filter sinkFilter("ffbuffersink");
    BufferSinkFilterContextPtr sinkFilterCtx;

    if (!sinkFilter.isValid())
    {
        clog << "Can't found sink filter: ffbuffersink" << endl;
        return FilterGraphPtr();
    }

    BufferSinkOpaque bufferSinkOpaque;
    vector<PixelFormat> allowedPixelFormats;
    allowedPixelFormats.push_back(dstPixelFormat);
    bufferSinkOpaque.setAllowedPixelFormats(allowedPixelFormats);

    if (graph->createFilter(sinkFilter, sinkFilterName, string(), bufferSinkOpaque) < 0)
    {
        clog << "Can't create filter 'ffbuffersink' in filter graph" << endl;
        return FilterGraphPtr();
    }

    sinkFilterCtx = filter_cast<BufferSinkFilterContext>(graph->getFilter(sinkFilterName));


    //
    // Output scale filter
    //
    FilterContextPtr scaleFilterCtx;
    if (srcWidth != dstWidth || srcHeight != dstHeight)
    {
        Filter scaleFilter("scale");

        if (!scaleFilter.isValid())
        {
            clog << "Filter is needed but not found: scale" << endl;
            return FilterGraphPtr();
        }

        stringstream ss;
        ss << dec << dstWidth << ":" << dstHeight << ":flags=0x" << hex << swsFlags;

        string scaleArgs = ss.str();
                //boost::str(boost::format("%d:%d:flags=0x%X") % dstWidth % dstHeight % swsFlags);

        if (graph->createFilter(scaleFilter, scaleFilterName, scaleArgs) < 0)
        {
            clog << "Can't create filter in filter graph: scale" << endl;
            return FilterGraphPtr();
        }

        scaleFilterCtx = graph->getFilter(scaleFilterName);
    }

    //
    // Output format filter
    //
    FilterContextPtr formatFilterCtx;
    if (srcPixelFormat != dstPixelFormat)
    {
        Filter formatFilter("format");

        if (!formatFilter.isValid())
        {
            clog << "Filter is needed but not found: format" << endl;
            return FilterGraphPtr();
        }


        string formatArgs = string(av_get_pix_fmt_name(dstPixelFormat));
                //boost::str(boost::format("%s") % av_get_pix_fmt_name(dstPixelFormat));

        if (graph->createFilter(formatFilter, formatFilterName, formatArgs) < 0)
        {
            clog << "Can't create filter in filter graph: format" << endl;
            return FilterGraphPtr();
        }

        formatFilterCtx = graph->getFilter(formatFilterName);
    }

    //
    // Link filters
    //
    FilterContextPtr tmp = srcFilterCtx;
    // TODO remove code duplication
    if (scaleFilterCtx)
    {
        if (tmp->link(0, scaleFilterCtx, 0) < 0)
        {
            clog << "Can't link filters: " << tmp->getName() << " -> " << scaleFilterCtx->getName() << endl;
            return FilterGraphPtr();
        }

        tmp = scaleFilterCtx;
    }

    if (formatFilterCtx)
    {
        if (tmp->link(0, formatFilterCtx, 0) < 0)
        {
            clog << "Can't link filters: " << tmp->getName() << " -> " << formatFilterCtx->getName() << endl;
            return FilterGraphPtr();
        }

        tmp = formatFilterCtx;
    }

    // Parse graph description
    if (graph->parse(graphDescription, tmp, sinkFilterCtx) < 0)
    {
        clog << "Can't parse graph description: " << graphDescription << endl;
        return FilterGraphPtr();
    }

    if (graph->config() < 0)
    {
        clog << "Can't configure graph" << endl;
        return FilterGraphPtr();
    }

    graph->setSrcFilter(srcFilterCtx);
    graph->setSinkFilter(sinkFilterCtx);

    return graph;
}

void FilterGraph::addManagedWrapper(AVFilterContext *ctx)
{
    if (filtersMapping.find(ctx) == filtersMapping.end())
    {
        FilterContextPtr wrapper(new FilterContext());
        wrapper->setManaged(true);
        wrapper->wrap(ctx);
        filtersMapping[ctx] = wrapper;
    }
}

FilterContextPtr FilterGraph::getFilter(AVFilterContext *ctx)
{
    if (!ctx)
        return FilterContextPtr();

    if (filtersMapping.find(ctx) == filtersMapping.end())
    {
        FilterContextPtr tmp = getFilter(string(ctx->name));
        if (!tmp || tmp->getAVFilterContext() != ctx)
            return FilterContextPtr();
    }

    return filtersMapping[ctx];
}

void FilterGraph::setSrcFilter(const BufferSrcFilterContextPtr &filterCtx)
{
    if (!filterCtx)
        return;

    // TODO check for present in graph
    srcFilterContext = filterCtx;
}

void FilterGraph::setSinkFilter(const BufferSinkFilterContextPtr &filterCtx)
{
    if (!filterCtx)
        return;

    // TODO check for present in graph
    sinkFilterContext = filterCtx;
}

} // namespace av
