#include <cassert>

#include "buffersink.h"

using namespace std;

namespace av {

struct BufferSinkFilterContextPriv
{
    BufferSinkFilterContextPriv()
        : frameSize(0)
    {}

    unsigned frameSize;
};

BufferSinkFilterContext::BufferSinkFilterContext()
    : priv(new BufferSinkFilterContextPriv)
{
}

BufferSinkFilterContext::BufferSinkFilterContext(const Filter &filter, const string &name)
    : priv(new BufferSinkFilterContextPriv)
{
    if (isFilterValid(filter))
    {
        reinitInt(filter, name);
    }
}

BufferSinkFilterContext::BufferSinkFilterContext(const FilterContext &baseContext)
    : priv(new BufferSinkFilterContextPriv)
{
    if (baseContext && isFilterValid(baseContext.getFilter()))
    {
        makeRef(baseContext);
    }
}

BufferSinkFilterContext::~BufferSinkFilterContext()
{
    delete priv;
}

int BufferSinkFilterContext::reinit(const Filter &filter, const string &name)
{
    if (isFilterValid(filter))
    {
        return reinitInt(filter, name);
    }

    clog << "You pass invalid filter for context: " << (filter.isValid() ? filter.getName() : "(null)") << endl;

    return -1;
}


void BufferSinkFilterContext::setFrameSize(unsigned size)
{
    assert(isValid());

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    priv->frameSize = size;
#else
    av_buffersink_set_frame_size(getAVFilterContext(), size);
#endif
}

int BufferSinkFilterContext::getBufferRef(FilterBufferRef &ref, int flags)
{
    assert(isValid());

    AVFilterBufferRef *rawRef;

    int stat = -1;

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
    if (priv->frameSize > 0)
    {
        stat = av_buffersink_read_samples(getAVFilterContext(), &rawRef, priv->frameSize);
    }
    else
#endif
    {
        stat = av_buffersink_get_buffer_ref(getAVFilterContext(), &rawRef, flags);
    }

    if (stat >= 0)
    {
        ref.reset(rawRef);
    }

    return stat;
}

int BufferSinkFilterContext::pollFrames()
{
    assert(isValid());

    return av_buffersink_poll_frame(getAVFilterContext());
}

Rational BufferSinkFilterContext::getFrameRate()
{
    assert(isValid());

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
#pragma message ( "BufferSink get frame rate functionality does not present on FFMPEG prior 1.0. " \
                  "If you will be use this functionality in code assert will be called and apllication will be ended" )
    assert(0 && "BufferSink get frame rate functionality does not present on FFMPEG prior 1.0");
#else
    return av_buffersink_get_frame_rate(getAVFilterContext());
#endif
}

bool BufferSinkFilterContext::isFilterValid(const Filter &filter)
{
    if (filter.isValid())
    {
        if (filter.getName() == "buffersink" ||
            filter.getName() == "abuffersink" ||
            filter.getName() == "ffbuffersink" ||
            filter.getName() == "ffabuffersink")
        {
            return true;
        }
    }

    return false;
}

void BufferSinkOpaque::setAllowedPixelFormats(const std::vector<PixelFormat> &pixelFormats)
{
    this->pixelFormats = pixelFormats;

    // Last element must be PIX_FMT_NONE
    if (this->pixelFormats.empty() ||
        this->pixelFormats[this->pixelFormats.size() - 1] != PIX_FMT_NONE)
    {
        this->pixelFormats.push_back(PIX_FMT_NONE);
    }

    getTypedOpaque()->pixel_fmts = this->pixelFormats.data();
}

const std::vector<PixelFormat> &BufferSinkOpaque::getAllowedPixelFormats() const
{
    return pixelFormats;
}

void ABufferSinkOpaque::setAllowedPixelFormats(const std::vector<AVSampleFormat> &sampleFormats)
{
    this->sampleFormats = sampleFormats;

    // Last element must be AV_SAMPLE_FMT_NONE
    if (this->sampleFormats.empty() ||
        this->sampleFormats[this->sampleFormats.size() - 1] != AV_SAMPLE_FMT_NONE)
    {
        this->sampleFormats.push_back(AV_SAMPLE_FMT_NONE);
    }

    getTypedOpaque()->sample_fmts = this->sampleFormats.data();
}

const std::vector<AVSampleFormat> &ABufferSinkOpaque::getAllowedSampleFormats() const
{
    return sampleFormats;
}

void ABufferSinkOpaque::setAllowedChannelLayouts(const std::vector<int64_t> &channelLayouts)
{
    this->channelLayouts = channelLayouts;

    // Last element must be -1
    if (this->channelLayouts.empty() ||
        this->channelLayouts[this->channelLayouts.size() - 1] != -1)
    {
        this->channelLayouts.push_back(-1);
    }

    getTypedOpaque()->channel_layouts = channelLayouts.data();
}

const std::vector<int64_t> ABufferSinkOpaque::getAllowedChannelLayouts() const
{
    return channelLayouts;
}

} // namespace av
