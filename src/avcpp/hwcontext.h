#pragma once

/**
 * Hardware devices, hardware frames and hardware codec configuration.
 *
 * @file
 * @ingroup hw
 */

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "avcompat.h"
#include "averror.h"
#include "buffer.h"
#include "codec.h"
#include "dictionary.h"
#include "pixelformat.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace av {
namespace hw {

/**
 * @brief How an encoder receives hardware frames.
 */
enum class EncodingMode
{
    Auto,   ///< Prefer a hardware frames context, fall back to a device context.
    Frames, ///< Use AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX only.
    Device, ///< Use AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX only.
};

class DeviceContext;
class FramesContext;
class PixelFormatList;

/**
 * @brief Wrapper for AVHWDeviceType.
 *
 * Device types are looked up by name at runtime, so the wrapper works with
 * FFmpeg versions that do not know newer enum values.
 */
class DeviceType
{
public:
    constexpr DeviceType(AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE) noexcept
        : m_type(type)
    {}

    operator AVHWDeviceType() const noexcept { return m_type; }

    AVHWDeviceType get() const noexcept { return m_type; }

    bool isNone() const noexcept { return m_type == AV_HWDEVICE_TYPE_NONE; }

    /**
     * @brief FFmpeg name of the device type, or "none".
     */
    const char* name() const noexcept;

    static DeviceType fromName(const char* name) noexcept;
    static DeviceType fromName(const std::string& name) noexcept;

private:
    AVHWDeviceType m_type = AV_HWDEVICE_TYPE_NONE;
};

/**
 * @brief Owner of an AVHWDeviceContext.
 *
 * The context is reference counted with AVBufferRef. Copying the wrapper only
 * adds a reference. Use raw() or bufferRef() to pass the context to FFmpeg.
 */
class DeviceContext
{
    friend class FramesContext;

public:
    DeviceContext() = default;

    static DeviceContext create(DeviceType type,
                                const char* device = nullptr,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    static DeviceContext create(DeviceType type,
                                const std::string& device,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    static DeviceContext derive(const DeviceContext& source,
                                DeviceType type,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    /**
     * @brief Add a reference to an existing AVBufferRef.
     */
    static DeviceContext wrap(AVBufferRef* ref) noexcept;

    bool isValid() const noexcept;
    DeviceType type() const noexcept;

    AVHWDeviceContext* raw() const noexcept;
    AVBufferRef* bufferRef() const noexcept;

private:
    explicit DeviceContext(BufferRef&& ref) noexcept
        : m_ref(std::move(ref))
    {}

    BufferRef m_ref;
};

/**
 * @brief Owner of an AVHWFramesContext.
 */
class FramesContext
{
public:
    FramesContext() = default;

    /**
     * @brief Creates and initializes a hardware frames context.
     *
     * @param initialPoolSize Zero asks FFmpeg to choose the pool size. FFmpeg
     *        4.x QSV requires a fixed size, so avcpp uses 20 surfaces in that
     *        case.
     */
    static FramesContext create(const DeviceContext& device,
                                PixelFormat format,
                                PixelFormat swFormat,
                                int width,
                                int height,
                                int initialPoolSize = 0,
                                OptionalErrorCode ec = throws());

    /**
     * @brief Add a reference to an existing AVBufferRef.
     */
    static FramesContext wrap(AVBufferRef* ref) noexcept;

    bool isValid() const noexcept;
    DeviceContext device() const noexcept;
    PixelFormat format() const noexcept;
    PixelFormat swFormat() const noexcept;
    int width() const noexcept;
    int height() const noexcept;

    PixelFormatList transferFormats(AVHWFrameTransferDirection direction,
                                    OptionalErrorCode ec = throws()) const;

    AVHWFramesContext* raw() const noexcept;
    AVBufferRef* bufferRef() const noexcept;

private:
    explicit FramesContext(BufferRef&& ref) noexcept
        : m_ref(std::move(ref))
    {}

    BufferRef m_ref;
};

/**
 * @brief Non-owning view over a contiguous array.
 */
template<typename T>
class RangeView
{
public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    RangeView() = default;
    RangeView(T* data, size_t size) noexcept
        : m_data(data), m_size(size)
    {}

    T* begin() const noexcept { return m_data; }
    T* end() const noexcept { return m_data + m_size; }
    size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    T& operator[](size_t index) const noexcept { return m_data[index]; }

    std::vector<T> toVector() const
    {
        return m_data ? std::vector<T>(m_data, m_data + m_size) : std::vector<T>();
    }

private:
    T*     m_data = nullptr;
    size_t m_size = 0;
};

/**
 * @brief Non-owning iterator over AVHWDeviceType values.
 */
class DeviceTypeRange
{
public:
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = DeviceType;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = DeviceType;

        iterator() = default;
        explicit iterator(DeviceType type) noexcept : m_type(type) {}

        DeviceType operator*() const noexcept { return m_type; }
        iterator& operator++() noexcept
        {
            m_type = DeviceType(av_hwdevice_iterate_types(m_type.get()));
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const noexcept { return m_type.get() == other.m_type.get(); }
        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

    private:
        DeviceType m_type;
    };

    iterator begin() const noexcept
    {
        return iterator(DeviceType(av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE)));
    }
    iterator end() const noexcept { return iterator(DeviceType(AV_HWDEVICE_TYPE_NONE)); }
};

/**
 * @brief All hardware device types compiled into FFmpeg.
 *
 * This lists compiled backends, not present hardware. Use probeDevices() or
 * isDeviceAvailable() to check whether a device can actually be created.
 */
DeviceTypeRange allDeviceTypes() noexcept;

struct DeviceProbe
{
    DeviceType     type;
    bool           available = false;
    std::error_code error;
};

/**
 * @brief Lazy range that probes every compiled device type.
 *
 * A probe creates a temporary device context and destroys it immediately.
 * Availability is reported per type. Creation is the only reliable check:
 * av_hwdevice_iterate_types() lists compiled backends, not present hardware.
 *
 * @param device Backend-specific device selector passed to
 *               av_hwdevice_ctx_create(). It is a string and not
 *               std::filesystem::path on purpose: the value identifies a
 *               device, and it is not necessarily a node in the filesystem.
 *               Depending on the backend it can be:
 *               - a filesystem path (VAAPI, DRM: "/dev/dri/renderD128");
 *               - a numeric index (CUDA: "1");
 *               - a display or adapter name (D3D11VA, VideoToolbox);
 *               - an empty string or nullptr for the default device.
 *               A path type would wrongly imply a filesystem node and
 *               cannot represent the other forms.
 *
 * @note The const char* overload borrows the pointer. The caller must keep
 *       the string alive for the whole iteration. The std::string overload
 *       stores a copy inside the range.
 *
 * @see isDeviceAvailable(), allDeviceTypes()
 */
class DeviceProbeRange
{
public:
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = DeviceProbe;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const DeviceProbe*;
        using reference         = const DeviceProbe&;

        iterator() = default;
        iterator(DeviceType type, const char* device)
            : m_type(type), m_device(device)
        {
            probe();
        }

        reference operator*() const noexcept { return m_probe; }
        pointer operator->() const noexcept { return &m_probe; }

        iterator& operator++() noexcept
        {
            if (!m_type.isNone())
                m_type = DeviceType(av_hwdevice_iterate_types(m_type.get()));
            probe();
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const noexcept { return m_type.get() == other.m_type.get(); }
        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

    private:
        void probe() noexcept
        {
            m_probe = DeviceProbe{};
            m_probe.type = m_type;
            if (m_type.isNone())
                return;

            AVBufferRef* ref = nullptr;
            int ret = av_hwdevice_ctx_create(&ref, m_type.get(), m_device, nullptr, 0);
            if (ret < 0) {
                m_probe.error = make_ffmpeg_error(ret);
                return;
            }
            m_probe.available = true;
            av_buffer_unref(&ref);
        }

        DeviceType  m_type;
        const char* m_device = nullptr;
        DeviceProbe m_probe;
    };

    DeviceProbeRange() = default;
    explicit DeviceProbeRange(const char* device) noexcept
        : m_device(device)
    {}

    explicit DeviceProbeRange(const std::string& device)
        : m_owned(device), m_device(m_owned->c_str())
    {}

    DeviceProbeRange(const DeviceProbeRange& other)
        : m_owned(other.m_owned),
          m_device(other.m_owned ? m_owned->c_str() : other.m_device)
    {}

    DeviceProbeRange(DeviceProbeRange&& other) noexcept
        : m_owned(std::move(other.m_owned)),
          m_device(m_owned ? m_owned->c_str() : other.m_device)
    {
        other.m_device = nullptr;
    }

    DeviceProbeRange& operator=(DeviceProbeRange other)
    {
        swap(other);
        return *this;
    }

    void swap(DeviceProbeRange& other) noexcept
    {
        using std::swap;
        swap(m_owned, other.m_owned);
        swap(m_device, other.m_device);
        if (m_owned)
            m_device = m_owned->c_str();
        if (other.m_owned)
            other.m_device = other.m_owned->c_str();
    }

    iterator begin() const
    {
        return iterator(DeviceType(av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE)), m_device);
    }
    iterator end() const { return iterator(); }

private:
    std::optional<std::string> m_owned;
    const char*                m_device = nullptr;
};

DeviceProbeRange probeDevices(const char* device = nullptr);
DeviceProbeRange probeDevices(const std::string& device);

bool isDeviceAvailable(DeviceType type, const char* device = nullptr);
bool isDeviceAvailable(DeviceType type, const std::string& device);

/**
 * @brief Non-owning view over a null-terminated AVPixelFormat array.
 */
class PixelFormatRange
{
public:
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = PixelFormat;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = PixelFormat;

        iterator() = default;
        explicit iterator(const AVPixelFormat* ptr) noexcept : m_ptr(ptr) {}

        PixelFormat operator*() const noexcept { return PixelFormat(*m_ptr); }
        iterator& operator++() noexcept
        {
            ++m_ptr;
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const noexcept { return m_ptr == other.m_ptr; }
        bool operator!=(const iterator& other) const noexcept { return m_ptr != other.m_ptr; }

    private:
        const AVPixelFormat* m_ptr = nullptr;
    };

    PixelFormatRange() = default;
    explicit PixelFormatRange(const AVPixelFormat* formats) noexcept;

    iterator begin() const noexcept { return iterator(m_begin); }
    iterator end() const noexcept { return iterator(m_end); }
    size_t size() const noexcept { return static_cast<size_t>(m_end - m_begin); }
    bool empty() const noexcept { return m_begin == m_end; }
    PixelFormat operator[](size_t index) const noexcept { return PixelFormat(m_begin[index]); }

    std::vector<PixelFormat> toVector() const;

private:
    const AVPixelFormat* m_begin = nullptr;
    const AVPixelFormat* m_end   = nullptr;
};

/**
 * @brief Owner of an AVPixelFormat array allocated by FFmpeg.
 *
 * The class is a range itself. Iteration returns a non-owning view.
 */
class PixelFormatList
{
public:
    using iterator = PixelFormatRange::iterator;

    PixelFormatList() = default;
    explicit PixelFormatList(AVPixelFormat* formats) noexcept
        : m_formats(formats)
    {}
    ~PixelFormatList();

    PixelFormatList(PixelFormatList&& other) noexcept
        : m_formats(other.m_formats)
    {
        other.m_formats = nullptr;
    }
    PixelFormatList& operator=(PixelFormatList&& other) noexcept;

    PixelFormatList(const PixelFormatList&) = delete;
    PixelFormatList& operator=(const PixelFormatList&) = delete;

    PixelFormatRange range() const noexcept { return PixelFormatRange(m_formats); }
    iterator begin() const noexcept { return range().begin(); }
    iterator end() const noexcept { return range().end(); }
    size_t size() const noexcept { return range().size(); }
    bool empty() const noexcept { return range().empty(); }
    std::vector<PixelFormat> toVector() const { return range().toVector(); }

private:
    AVPixelFormat* m_formats = nullptr;
};

/**
 * @brief Owner of AVHWFramesConstraints.
 *
 * hwFormats() and swFormats() return non-owning views into the owned struct.
 */
class FrameConstraints
{
public:
    FrameConstraints() = default;

    static FrameConstraints query(const DeviceContext& device,
                                  OptionalErrorCode ec = throws());

    bool isValid() const noexcept { return static_cast<bool>(m_raw); }
    PixelFormatRange hwFormats() const noexcept;
    PixelFormatRange swFormats() const noexcept;
    int minWidth() const noexcept;
    int maxWidth() const noexcept;
    int minHeight() const noexcept;
    int maxHeight() const noexcept;

private:
    struct Deleter
    {
        void operator()(AVHWFramesConstraints* ptr) const noexcept;
    };

    std::unique_ptr<AVHWFramesConstraints, Deleter> m_raw;
};

/**
 * @brief Queries hardware frames constraints for a device.
 *
 * The returned object owns the FFmpeg constraints; hwFormats() and swFormats()
 * are non-owning views into it.
 */
FrameConstraints frameConstraints(const DeviceContext& device,
                                  OptionalErrorCode ec = throws());

/**
 * @brief Non-owning iterator over AVCodecHWConfig entries of a codec.
 */
class CodecConfigRange
{
public:
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = AVCodecHWConfig;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const AVCodecHWConfig*;
        using reference         = const AVCodecHWConfig&;

        iterator() = default;
        iterator(const AVCodec* codec, int index) noexcept
            : m_codec(codec), m_index(index)
        {
            refresh();
        }

        reference operator*() const noexcept { return m_value; }
        pointer operator->() const noexcept { return &m_value; }

        iterator& operator++() noexcept
        {
            ++m_index;
            refresh();
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const noexcept
        {
            if (m_codec != other.m_codec)
                return false;
            if (!m_valid || !other.m_valid)
                return m_valid == other.m_valid;
            return m_index == other.m_index;
        }
        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

    private:
        void refresh() noexcept
        {
            m_valid = false;
            if (!m_codec || m_index < 0)
                return;
            const AVCodecHWConfig* cfg = avcodec_get_hw_config(m_codec, m_index);
            if (!cfg)
                return;
            m_value = *cfg;
            m_valid = true;
        }

        const AVCodec*  m_codec = nullptr;
        int             m_index = 0;
        bool            m_valid = false;
        AVCodecHWConfig m_value{};
    };

    CodecConfigRange() = default;
    explicit CodecConfigRange(const Codec& codec) noexcept
        : m_codec(codec.raw())
    {}

    iterator begin() const { return iterator(m_codec, 0); }
    iterator end() const { return iterator(m_codec, -1); }

    bool empty() const { return begin() == end(); }

private:
    const AVCodec* m_codec = nullptr;
};

CodecConfigRange codecConfigs(const Codec& codec) noexcept;

std::optional<AVCodecHWConfig> findCodecConfig(
    const Codec& codec,
    DeviceType type,
    int methodsMask = AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX |
                      AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX);

/**
 * @brief Non-owning range over codecs found with av_codec_iterate().
 */
class CodecRange
{
public:
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = Codec;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = Codec;

        iterator() = default;
        iterator(AVCodecID id, DeviceType type, bool encoders)
            : m_id(id), m_type(type), m_encoders(encoders)
        {
            advance();
        }

        Codec operator*() const noexcept { return Codec(m_codec); }
        iterator& operator++() noexcept
        {
            advance();
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const noexcept { return m_codec == other.m_codec; }
        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

    private:
        void advance() noexcept
        {
            for (;;) {
                m_codec = av_codec_iterate(&m_opaque);
                if (!m_codec)
                    return;
                if (m_encoders ? !av_codec_is_encoder(m_codec) : !av_codec_is_decoder(m_codec))
                    continue;
                if (m_id != AV_CODEC_ID_NONE && m_codec->id != m_id)
                    continue;
                if (!codecSupportsDevice(m_codec, m_type.get(), m_encoders))
                    continue;
                return;
            }
        }

        static bool codecSupportsDevice(const AVCodec* codec, AVHWDeviceType type, bool encoders) noexcept;

        AVCodecID   m_id = AV_CODEC_ID_NONE;
        DeviceType  m_type;
        bool        m_encoders = false;
        void*       m_opaque = nullptr;
        const AVCodec* m_codec = nullptr;
    };

    CodecRange() = default;
    CodecRange(AVCodecID id, DeviceType type, bool encoders) noexcept
        : m_id(id), m_type(type), m_encoders(encoders)
    {}

    iterator begin() const { return iterator(m_id, m_type, m_encoders); }
    iterator end() const { return iterator(); }

private:
    AVCodecID  m_id = AV_CODEC_ID_NONE;
    DeviceType m_type;
    bool       m_encoders = false;
};

CodecRange findDecoders(AVCodecID id, DeviceType type) noexcept;
CodecRange findEncoders(AVCodecID id, DeviceType type) noexcept;
Codec findDecoder(AVCodecID id, DeviceType type) noexcept;
Codec findEncoder(AVCodecID id, DeviceType type) noexcept;

/**
 * @brief Hardware pixel format used by a device type.
 *
 * The lookup is name based, so the function does not depend on enum values
 * that may be missing in older FFmpeg versions.
 */
PixelFormat hwPixelFormat(DeviceType type) noexcept;

} // namespace hw
} // namespace av
