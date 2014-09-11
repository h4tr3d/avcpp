#ifndef AV_BUFFERSINK_H
#define AV_BUFFERSINK_H

#include <stdint.h>
#include <memory>

#include "../ffmpeg.h"
#include "../rational.h"
#include "../filtercontext.h"
#include "../filterbufferref.h"
#include "../filteropaque.h"

namespace av {

class BufferSinkFilterContext : public FilterContext
{
public:
    BufferSinkFilterContext();
    explicit BufferSinkFilterContext(const Filter& filter, const std::string &name = std::string());
    explicit BufferSinkFilterContext(const FilterContext& baseContext);
    ~BufferSinkFilterContext();

    virtual  int reinit(const Filter &filter, const std::string &name);

    void     setFrameSize(unsigned size);
    int      getBufferRef(FilterBufferRef& ref, int flags = 0);
    int      pollFrames();
    Rational getFrameRate();

    static bool isFilterValid(const Filter& filter);

    BufferSinkFilterContext(const BufferSinkFilterContext &other) = delete;
    void operator=(const BufferSinkFilterContext &rhs) = delete;

private:
    std::unique_ptr<struct BufferSinkFilterContextPriv> m_priv;
};

//
// Pointers
//
typedef std::shared_ptr<BufferSinkFilterContext> BufferSinkFilterContextPtr;
typedef std::weak_ptr<BufferSinkFilterContext> BufferSinkFilterContextWPtr;


//
// Casting
//
template <>
inline std::shared_ptr<BufferSinkFilterContext> filter_cast(const FilterContextPtr &ctx)
{
    if (ctx && ctx->isValid() && BufferSinkFilterContext::isFilterValid(ctx->getFilter()))
    {
        BufferSinkFilterContextPtr result;
        result = std::dynamic_pointer_cast<BufferSinkFilterContext>(ctx);
        if (result)
        {
            return result;
        }
        else
        {
            return std::make_shared<BufferSinkFilterContext>(*ctx);
        }
    }
    return BufferSinkFilterContextPtr();
}


//
// Filter opaque data
//

struct BufferSinkOpaqueAllocator
{
    AVBufferSinkParams * operator() ()
    {
        return av_buffersink_params_alloc();
    }
};

struct ABufferSinkOpaqueAllocator
{
    AVABufferSinkParams * operator() ()
    {
        return av_abuffersink_params_alloc();
    }
};

struct BufferSinkOpaqueDeleter
{
    void operator() (void * ptr)
    {
        av_free(ptr);
    }
};

/**
 * Opaque data for buffersink/ffbuffersink video filters
 */
class BufferSinkOpaque :
        public ManagedFilterOpaque<
        AVBufferSinkParams,
        BufferSinkOpaqueAllocator,
        BufferSinkOpaqueDeleter>
{
public:
    void setAllowedPixelFormats(const std::vector<PixelFormat>& pixelFormats);
    const std::vector<PixelFormat>& getAllowedPixelFormats() const;

private:
    std::vector<PixelFormat> pixelFormats;
};


/**
 * Opaque data for abuffersink/ffabuffersink audio filters
 */
class ABufferSinkOpaque :
        public ManagedFilterOpaque<
        AVABufferSinkParams,
        ABufferSinkOpaqueAllocator,
        BufferSinkOpaqueDeleter>
{
public:

    void setAllowedPixelFormats(const std::vector<AVSampleFormat>& sampleFormats);
    const std::vector<AVSampleFormat>& getAllowedSampleFormats() const;

    void setAllowedChannelLayouts(const std::vector<int64_t> &channelLayouts);
    const std::vector<int64_t> getAllowedChannelLayouts() const;


private:
    std::vector<AVSampleFormat> sampleFormats;
    std::vector<int64_t>        channelLayouts;
};



} // namespace av

#endif // AV_BUFFERSINK_H
