#include <cstring>

#include "hwcontext.h"

extern "C" {
#include <libavutil/mem.h>
}

using namespace std;

namespace av {
namespace hw {

//
// DeviceType
//

const char* DeviceType::name() const noexcept
{
    const char* typeName = av_hwdevice_get_type_name(m_type);
    return typeName ? typeName : "none";
}

DeviceType DeviceType::fromName(const char* name) noexcept
{
    if (!name)
        return DeviceType();
    return DeviceType(av_hwdevice_find_type_by_name(name));
}

DeviceType DeviceType::fromName(const std::string& name) noexcept
{
    return fromName(name.c_str());
}

//
// DeviceContext
//

DeviceContext DeviceContext::create(DeviceType type,
                                    const char* device,
                                    const Dictionary& options,
                                    OptionalErrorCode ec)
{
    clear_if(ec);

    if (type.isNone()) {
        throws_if(ec, Errors::HardwareDeviceUnavailable);
        return {};
    }

    // av_hwdevice_ctx_create() removes the options it consumes from the
    // dictionary, so pass a copy and keep the caller's dictionary unchanged.
    AVDictionary* opts = nullptr;
    if (av_dict_copy(&opts, options.raw(), 0) < 0) {
        av_dict_free(&opts);
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }

    AVBufferRef* ref = nullptr;
    int ret = av_hwdevice_ctx_create(&ref, type.get(), device, opts, 0);
    av_dict_free(&opts);
    if (ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
        return {};
    }

    return DeviceContext(BufferRef::wrap(ref));
}

DeviceContext DeviceContext::create(DeviceType type,
                                    const std::string& device,
                                    const Dictionary& options,
                                    OptionalErrorCode ec)
{
    return create(type, device.c_str(), options, ec);
}

DeviceContext DeviceContext::derive(const DeviceContext& source,
                                    DeviceType type,
                                    const Dictionary& options,
                                    OptionalErrorCode ec)
{
    clear_if(ec);

    if (!source.isValid()) {
        throws_if(ec, Errors::HardwareDeviceInvalid);
        return {};
    }
    if (type.isNone()) {
        throws_if(ec, Errors::HardwareDeviceUnavailable);
        return {};
    }

    // Pass a copy: the backend may remove the options it consumes.
    AVDictionary* opts = nullptr;
    if (av_dict_copy(&opts, options.raw(), 0) < 0) {
        av_dict_free(&opts);
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }

    AVBufferRef* ref = nullptr;
    int ret;

#if AVCPP_API_HWDEVICE_DERIVED_OPTS
    ret = av_hwdevice_ctx_create_derived_opts(&ref,
                                              type.get(),
                                              source.bufferRef(),
                                              opts,
                                              0);
#else
    // Options are not supported by the old API.
    (void)options;
    ret = av_hwdevice_ctx_create_derived(&ref, type.get(), source.bufferRef(), 0);
#endif // if AVCPP_API_HWDEVICE_DERIVED_OPTS

    av_dict_free(&opts);
    if (ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
        return {};
    }

    return DeviceContext(BufferRef::wrap(ref));
}

DeviceContext DeviceContext::wrap(AVBufferRef* ref) noexcept
{
    if (!ref)
        return {};
    return DeviceContext(BufferRef::ref(ref));
}

bool DeviceContext::isValid() const noexcept
{
    return m_ref.raw() != nullptr;
}

DeviceType DeviceContext::type() const noexcept
{
    AVHWDeviceContext* device = raw();
    return device ? DeviceType(device->type) : DeviceType();
}

AVHWDeviceContext* DeviceContext::raw() const noexcept
{
    const AVBufferRef* ref = m_ref.raw();
    return ref ? reinterpret_cast<AVHWDeviceContext*>(ref->data) : nullptr;
}

AVBufferRef* DeviceContext::bufferRef() const noexcept
{
    return const_cast<AVBufferRef*>(m_ref.raw());
}

//
// FramesContext
//

FramesContext FramesContext::create(const DeviceContext& device,
                                    PixelFormat format,
                                    PixelFormat swFormat,
                                    int width,
                                    int height,
                                    int initialPoolSize,
                                    OptionalErrorCode ec)
{
    clear_if(ec);

    if (!device.isValid()) {
        throws_if(ec, Errors::HardwareDeviceInvalid);
        return {};
    }

    AVBufferRef* ref = av_hwframe_ctx_alloc(device.bufferRef());
    if (!ref) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }

    AVHWFramesContext* ctx = reinterpret_cast<AVHWFramesContext*>(ref->data);
    ctx->format    = format.get();
    ctx->sw_format = swFormat.get();
    ctx->width     = width;
    ctx->height    = height;
    if (initialPoolSize > 0)
        ctx->initial_pool_size = initialPoolSize;
    else if (initialPoolSize == 0 && device.type().get() == AV_HWDEVICE_TYPE_QSV)
        // FFmpeg 4.x QSV rejects a zero pool size ("QSV requires a fixed
        // frame pool size"). Use a small fixed pool when the caller did not
        // request a size.
        ctx->initial_pool_size = 20;

    int ret = av_hwframe_ctx_init(ref);
    if (ret < 0) {
        av_buffer_unref(&ref);
        throws_if(ec, ret, ffmpeg_category());
        return {};
    }

    return FramesContext(BufferRef::wrap(ref));
}

FramesContext FramesContext::wrap(AVBufferRef* ref) noexcept
{
    if (!ref)
        return {};
    return FramesContext(BufferRef::ref(ref));
}

bool FramesContext::isValid() const noexcept
{
    return m_ref.raw() != nullptr;
}

DeviceContext FramesContext::device() const noexcept
{
    AVHWFramesContext* ctx = raw();
    if (!ctx || !ctx->device_ref)
        return {};
    return DeviceContext(BufferRef::ref(ctx->device_ref));
}

PixelFormat FramesContext::format() const noexcept
{
    AVHWFramesContext* ctx = raw();
    return ctx ? PixelFormat(ctx->format) : PixelFormat(AV_PIX_FMT_NONE);
}

PixelFormat FramesContext::swFormat() const noexcept
{
    AVHWFramesContext* ctx = raw();
    return ctx ? PixelFormat(ctx->sw_format) : PixelFormat(AV_PIX_FMT_NONE);
}

int FramesContext::width() const noexcept
{
    AVHWFramesContext* ctx = raw();
    return ctx ? ctx->width : 0;
}

int FramesContext::height() const noexcept
{
    AVHWFramesContext* ctx = raw();
    return ctx ? ctx->height : 0;
}

PixelFormatList FramesContext::transferFormats(AVHWFrameTransferDirection direction,
                                               OptionalErrorCode ec) const
{
    clear_if(ec);

    if (!isValid()) {
        throws_if(ec, Errors::HardwareFramesInvalid);
        return {};
    }

    AVPixelFormat* formats = nullptr;
    int ret = av_hwframe_transfer_get_formats(const_cast<AVBufferRef*>(m_ref.raw()),
                                              direction,
                                              &formats,
                                              0);
    if (ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
        return {};
    }

    return PixelFormatList(formats);
}

AVHWFramesContext* FramesContext::raw() const noexcept
{
    const AVBufferRef* ref = m_ref.raw();
    return ref ? reinterpret_cast<AVHWFramesContext*>(ref->data) : nullptr;
}

AVBufferRef* FramesContext::bufferRef() const noexcept
{
    return const_cast<AVBufferRef*>(m_ref.raw());
}

//
// Device iteration and probing
//

DeviceTypeRange allDeviceTypes() noexcept
{
    return DeviceTypeRange();
}

DeviceProbeRange probeDevices(const char* device)
{
    return DeviceProbeRange(device);
}

DeviceProbeRange probeDevices(const std::string& device)
{
    return DeviceProbeRange(device);
}

bool isDeviceAvailable(DeviceType type, const char* device)
{
    if (type.isNone())
        return false;

    AVBufferRef* ref = nullptr;
    int ret = av_hwdevice_ctx_create(&ref, type.get(), device, nullptr, 0);
    if (ret < 0)
        return false;

    av_buffer_unref(&ref);
    return true;
}

bool isDeviceAvailable(DeviceType type, const std::string& device)
{
    return isDeviceAvailable(type, device.c_str());
}

//
// PixelFormatRange and PixelFormatList
//

PixelFormatRange::PixelFormatRange(const AVPixelFormat* formats) noexcept
{
    if (!formats)
        return;

    m_begin = formats;
    m_end   = formats;
    while (*m_end != AV_PIX_FMT_NONE)
        ++m_end;
}

std::vector<PixelFormat> PixelFormatRange::toVector() const
{
    std::vector<PixelFormat> res;
    res.reserve(size());
    for (const AVPixelFormat* fmt = m_begin; fmt != m_end; ++fmt)
        res.push_back(PixelFormat(*fmt));
    return res;
}

PixelFormatList::~PixelFormatList()
{
    if (m_formats)
        av_freep(&m_formats);
}

PixelFormatList& PixelFormatList::operator=(PixelFormatList&& other) noexcept
{
    if (this != &other) {
        if (m_formats)
            av_freep(&m_formats);
        m_formats       = other.m_formats;
        other.m_formats = nullptr;
    }
    return *this;
}

//
// FrameConstraints
//

void FrameConstraints::Deleter::operator()(AVHWFramesConstraints* ptr) const noexcept
{
    av_hwframe_constraints_free(&ptr);
}

FrameConstraints FrameConstraints::query(const DeviceContext& device, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!device.isValid()) {
        throws_if(ec, Errors::HardwareDeviceInvalid);
        return {};
    }

    AVHWFramesConstraints* constraints =
        av_hwdevice_get_hwframe_constraints(device.bufferRef(), nullptr);
    if (!constraints) {
        throws_if(ec, Errors::HardwareConfigNotFound);
        return {};
    }

    FrameConstraints res;
    res.m_raw.reset(constraints);
    return res;
}

FrameConstraints frameConstraints(const DeviceContext& device, OptionalErrorCode ec)
{
    return FrameConstraints::query(device, ec);
}

PixelFormatRange FrameConstraints::hwFormats() const noexcept
{
    if (!m_raw || !m_raw->valid_hw_formats)
        return PixelFormatRange();
    return PixelFormatRange(m_raw->valid_hw_formats);
}

PixelFormatRange FrameConstraints::swFormats() const noexcept
{
    if (!m_raw || !m_raw->valid_sw_formats)
        return PixelFormatRange();
    return PixelFormatRange(m_raw->valid_sw_formats);
}

int FrameConstraints::minWidth() const noexcept
{
    return m_raw ? m_raw->min_width : 0;
}

int FrameConstraints::maxWidth() const noexcept
{
    return m_raw ? m_raw->max_width : 0;
}

int FrameConstraints::minHeight() const noexcept
{
    return m_raw ? m_raw->min_height : 0;
}

int FrameConstraints::maxHeight() const noexcept
{
    return m_raw ? m_raw->max_height : 0;
}

//
// Codec hardware configurations
//

CodecConfigRange codecConfigs(const Codec& codec) noexcept
{
    return CodecConfigRange(codec);
}

std::optional<AVCodecHWConfig> findCodecConfig(const Codec& codec,
                                               DeviceType type,
                                               int methodsMask)
{
    for (const AVCodecHWConfig& cfg : codecConfigs(codec)) {
        if (cfg.device_type == type.get() && (cfg.methods & methodsMask))
            return cfg;
    }
    return std::nullopt;
}

bool CodecRange::iterator::codecSupportsDevice(const AVCodec* codec,
                                               AVHWDeviceType type,
                                               bool encoders) noexcept
{
    (void)encoders;

    constexpr int methods = AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX |
                            AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX;

    for (int index = 0;; ++index) {
        const AVCodecHWConfig* cfg = avcodec_get_hw_config(codec, index);
        if (!cfg)
            return false;
        if (cfg->device_type != type)
            continue;
        if (cfg->methods & methods)
            return true;
    }
}

CodecRange findDecoders(AVCodecID id, DeviceType type) noexcept
{
    return CodecRange(id, type, false);
}

CodecRange findEncoders(AVCodecID id, DeviceType type) noexcept
{
    return CodecRange(id, type, true);
}

Codec findDecoder(AVCodecID id, DeviceType type) noexcept
{
    for (Codec codec : findDecoders(id, type))
        return codec;
    return Codec();
}

Codec findEncoder(AVCodecID id, DeviceType type) noexcept
{
    for (Codec codec : findEncoders(id, type))
        return codec;
    return Codec();
}

//
// Hardware pixel formats
//

PixelFormat hwPixelFormat(DeviceType type) noexcept
{
    if (type.isNone())
        return PixelFormat(AV_PIX_FMT_NONE);

    struct Entry
    {
        const char* device;
        const char* format;
    };

    // Device types and pixel formats are matched by name, so the code does not
    // depend on enum values that may be missing in older FFmpeg versions.
    static const Entry entries[] = {
        { "vaapi",        "vaapi"        },
        { "qsv",          "qsv"          },
        { "cuda",         "cuda"         },
        { "vdpau",        "vdpau"        },
        { "dxva2",        "dxva2_vld"    },
        { "d3d11va",      "d3d11"        },
        { "d3d12va",      "d3d12"        },
        { "videotoolbox", "videotoolbox" },
        { "mediacodec",   "mediacodec"   },
        { "vulkan",       "vulkan"       },
        { "opencl",       "opencl"       },
        { "drm",          "drm_prime"    },
        { "amf",          "amf"          },
        { "ohcodec",      "ohcodec"      },
    };

    const char* typeName = type.name();
    for (const Entry& entry : entries) {
        if (std::strcmp(typeName, entry.device) == 0)
            return PixelFormat(entry.format);
    }

    return PixelFormat(AV_PIX_FMT_NONE);
}

} // namespace hw
} // namespace av
