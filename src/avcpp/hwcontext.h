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
    /**
     * Wrap an FFmpeg device type.
     *
     * @param type the FFmpeg type to wrap; the default means "none"
     */
    constexpr DeviceType(AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE) noexcept
        : m_type(type)
    {}

    /**
     * Implicit conversion to the FFmpeg type, so the wrapper can be passed to
     * FFmpeg calls unchanged.
     *
     * @return the wrapped AVHWDeviceType
     */
    operator AVHWDeviceType() const noexcept { return m_type; }

    /**
     * The wrapped FFmpeg type.
     *
     * @return the wrapped AVHWDeviceType
     */
    AVHWDeviceType get() const noexcept { return m_type; }

    /**
     * Whether the wrapper holds no device type.
     *
     * @return `true` when the wrapped value is `AV_HWDEVICE_TYPE_NONE`
     */
    bool isNone() const noexcept { return m_type == AV_HWDEVICE_TYPE_NONE; }

    /**
     * FFmpeg name of the device type, or "none".
     *
     * @return a static string owned by FFmpeg; do not free it
     */
    const char* name() const noexcept;

    /**
     * Look a device type up by its FFmpeg name.
     *
     * Lookup is by name on purpose: the code does not depend on enumeration
     * values that older FFmpeg versions do not have.
     *
     * @param name a null-terminated name such as "vaapi"; must not be null
     *
     * @return the matching type, or a "none" wrapper when FFmpeg does not know
     *         the name or the backend is not compiled in
     */
    static DeviceType fromName(const char* name) noexcept;
    /**
     * Look a device type up by its FFmpeg name.
     *
     * @param name a name such as "vaapi"
     *
     * @return the matching type, or a "none" wrapper when the name is unknown or
     *         the backend is not compiled in
     */
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

    /**
     * Create and open a hardware device.
     *
     * @param type    the device type to create
     * @param device  backend-specific selector: a path for VAAPI/DRM, an index
     *                for CUDA, an adapter name for D3D11VA, or nullptr for the
     *                default device
     * @param options backend-specific options passed to FFmpeg
     * @param ec      holder for the error code; `throws()` raises av::Exception
     *
     * @return a valid context, or an invalid one when the device cannot be created
     *
     * @note This overload borrows `device`; it must outlive the call. Use the
     *       `std::string` overload when the selector is a temporary.
     */
    static DeviceContext create(DeviceType type,
                                const char* device = nullptr,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    /**
     * Create and open a hardware device from a std::string selector.
     *
     * @param type    the device type to create
     * @param device  backend-specific selector; may be empty for the default
     * @param options backend-specific options passed to FFmpeg
     * @param ec      holder for the error code
     *
     * @return a valid context, or an invalid one when the device cannot be created
     */
    static DeviceContext create(DeviceType type,
                                const std::string& device,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    /**
     * Create a device derived from an existing one.
     *
     * A typical use is an OpenCL device on top of a VAAPI device, so both APIs
     * share the same frames. Do not derive a QSV device: the libmfx path can
     * crash.
     *
     * @param source  the device to derive from; must be valid
     * @param type    the type of the derived device
     * @param options backend-specific options
     * @param ec      holder for the error code
     *
     * @return a valid derived device, or an invalid one on failure
     */
    static DeviceContext derive(const DeviceContext& source,
                                DeviceType type,
                                const Dictionary& options = {},
                                OptionalErrorCode ec = throws());

    /**
     * Take a reference to an existing AVBufferRef.
     *
     * The reference count is incremented, so the caller keeps ownership of its
     * pointer and must still unref it. Never free a pointer that was passed here
     * and then keep using the returned context.
     *
     * @param ref the buffer to reference; may be null
     *
     * @return a wrapper that owns one reference, invalid when `ref` is null
     */
    static DeviceContext wrap(AVBufferRef* ref) noexcept;

    /**
     * Whether the wrapper holds a device.
     *
     * @return `true` when the underlying context is present
     */
    bool isValid() const noexcept;
    /**
     * The type of the device.
     *
     * @return the device type, or "none" for an invalid wrapper
     */
    DeviceType type() const noexcept;

    /**
     * The underlying FFmpeg context.
     *
     * @return the raw pointer, or nullptr for an invalid wrapper
     *
     * @note Borrowed. Do not free it and do not keep it after the wrapper dies.
     */
    AVHWDeviceContext* raw() const noexcept;
    /**
     * The reference-counted buffer that owns the context.
     *
     * @return the raw buffer reference, or nullptr for an invalid wrapper
     *
     * @note Borrowed by the wrapper. Pass it to FFmpeg APIs that take an
     *       AVBufferRef* without unreffing it afterwards.
     */
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
     * Create and initialize a hardware frames context (a pool of surfaces).
     *
     * @param device          the device the surfaces live on
     * @param format          the hardware surface format, usually
     *                        `hwPixelFormat(device.type())`
     * @param swFormat        the format used when a frame is transferred to
     *                        system memory; `AV_PIX_FMT_NONE` lets FFmpeg choose
     * @param width           frame width in pixels
     * @param height          frame height in pixels
     * @param initialPoolSize number of surfaces; zero lets FFmpeg choose
     * @param ec              holder for the error code
     *
     * @return a valid pool, or an invalid one on failure
     *
     * @note FFmpeg 4.x QSV requires a fixed pool size; avcpp uses 20 surfaces in
     *       that case. Check FrameConstraints::swFormats() before choosing
     *       `swFormat`: not every device supports NV12.
     */
    static FramesContext create(const DeviceContext& device,
                                PixelFormat format,
                                PixelFormat swFormat,
                                int width,
                                int height,
                                int initialPoolSize = 0,
                                OptionalErrorCode ec = throws());

    /**
     * Take a reference to an existing AVHWFrameContext buffer.
     *
     * @param ref the buffer to reference; may be null
     *
     * @return a wrapper that owns one reference, invalid when `ref` is null
     */
    static FramesContext wrap(AVBufferRef* ref) noexcept;

    /**
     * Whether the wrapper holds a pool.
     *
     * @return `true` when the underlying context is present
     */
    bool isValid() const noexcept;
    /**
     * The device this pool belongs to.
     *
     * @return a copy of the owning device wrapper, obtained from `device_ref`
     */
    DeviceContext device() const noexcept;
    /**
     * The hardware format of the surfaces in the pool.
     *
     * @return the surface format, for example `AV_PIX_FMT_VAAPI`
     */
    PixelFormat format() const noexcept;
    /**
     * The system-memory format used to transfer frames into and out of the pool.
     *
     * @return the software format, for example `AV_PIX_FMT_NV12`
     */
    PixelFormat swFormat() const noexcept;
    /**
     * Surface width.
     *
     * @return the width in pixels
     */
    int width() const noexcept;
    /**
     * Surface height.
     *
     * @return the height in pixels
     */
    int height() const noexcept;

    /**
     * Formats that frames can be transferred to or from this pool.
     *
     * @param direction `AV_HWFRAME_TRANSFER_DIRECTION_TO` for uploads into the
     *                  pool, `AV_HWFRAME_TRANSFER_DIRECTION_FROM` for downloads
     * @param ec        holder for the error code
     *
     * @return an owning list of formats; the list is empty when the device does
     *         not report any
     *
     * @note The returned object owns the array FFmpeg allocated. Its `range()`
     *       view is only valid while the list is alive.
     */
    PixelFormatList transferFormats(AVHWFrameTransferDirection direction,
                                    OptionalErrorCode ec = throws()) const;

    /**
     * The underlying FFmpeg frames context.
     *
     * @return the raw pointer, or nullptr for an invalid wrapper
     *
     * @note Borrowed. Do not free it and do not keep it after the wrapper dies.
     */
    AVHWFramesContext* raw() const noexcept;
    /**
     * The reference-counted buffer that owns the frames context.
     *
     * @return the raw buffer reference, or nullptr for an invalid wrapper
     *
     * @note Borrowed by the wrapper. This is the pointer to pass to
     *       `AVCodecContext::hw_frames_ctx`.
     */
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
    /// Element type of the view.
    using value_type = T;
    /// Iterator type of the view.
    using iterator = T*;
    /// Const iterator type of the view.
    using const_iterator = const T*;

    /**
     * Create an empty view.
     */
    RangeView() = default;
    /**
     * Wrap an existing array.
     *
     * @param data pointer to the first element; may be null when `size` is zero
     * @param size number of elements
     *
     * @note Non-owning: the array must outlive the view.
     */
    RangeView(T* data, size_t size) noexcept
        : m_data(data), m_size(size)
    {}

    /**
     * First element.
     *
     * @return a pointer to the first element, or nullptr when empty
     */
    T* begin() const noexcept { return m_data; }
    /**
     * One past the last element.
     *
     * @return the end pointer of the view
     */
    T* end() const noexcept { return m_data + m_size; }
    /**
     * Number of elements.
     *
     * @return the element count
     */
    size_t size() const noexcept { return m_size; }
    /**
     * Whether the view has no elements.
     *
     * @return `true` when the element count is zero
     */
    bool empty() const noexcept { return m_size == 0; }
    /**
     * Element access.
     *
     * @param index zero-based index; must be less than size()
     *
     * @return a reference into the viewed array
     */
    T& operator[](size_t index) const noexcept { return m_data[index]; }

    /**
     * Copy the view into a vector.
     *
     * @return a vector with the same elements; empty for an empty view
     */
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
    /**
     * Forward iterator over the device types FFmpeg was built with.
     */
    class iterator
    {
    public:
        /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
        /// Type produced by dereferencing.
        using value_type        = DeviceType;
        /// Distance type required by the iterator traits.
        using difference_type   = std::ptrdiff_t;
        /// No pointer-to-element type: the value is produced on dereference.
        using pointer           = void;
        /// Dereference result type.
        using reference         = DeviceType;

        /**
         * End iterator.
         */
        iterator() = default;
        /**
         * Start the iteration at the given type.
         *
         * @param type the first type to visit
         */
        explicit iterator(DeviceType type) noexcept : m_type(type) {}

        /**
         * Current type.
         *
         * @return the type at the current position
         */
        DeviceType operator*() const noexcept { return m_type; }
        /**
         * Advance to the next type.
         *
         * @return this iterator
         */
        iterator& operator++() noexcept
        {
            m_type = DeviceType(av_hwdevice_iterate_types(m_type.get()));
            return *this;
        }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when both point at the same type
         */
        bool operator==(const iterator& other) const noexcept { return m_type.get() == other.m_type.get(); }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when the iterators point at different types
         */
        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

    private:
        DeviceType m_type;
    };

    /**
     * First compiled device type.
     *
     * @return an iterator at the first type reported by FFmpeg
     */
    iterator begin() const noexcept
    {
        return iterator(DeviceType(av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE)));
    }
    /**
     * End of the range.
     *
     * @return the past-the-end iterator
     */
    iterator end() const noexcept { return iterator(DeviceType(AV_HWDEVICE_TYPE_NONE)); }
};

/**
 * All hardware device types compiled into FFmpeg.
 *
 * This lists compiled backends, not present hardware. Use probeDevices() or
 * isDeviceAvailable() to check whether a device can actually be created.
 *
 * @return a non-owning range over the compiled types
 */
DeviceTypeRange allDeviceTypes() noexcept;

/**
 * Result of probing one device type: whether it could be created, and why not.
 */
struct DeviceProbe
{
    /**
     * The device type that was probed.
     */
    DeviceType     type;
    /**
     * Whether a device of this type could be created.
     */
    bool           available = false;
    /**
     * Why the probe failed; empty when `available` is true.
     */
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
    /**
     * Forward iterator that creates and destroys one device per step.
     */
    class iterator
    {
    public:
        /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
        /// Type produced by dereferencing.
        using value_type        = DeviceProbe;
        /// Distance type required by the iterator traits.
        using difference_type   = std::ptrdiff_t;
        /// Pointer to the probe result.
        using pointer           = const DeviceProbe*;
        /// Reference to the probe result.
        using reference         = const DeviceProbe&;

        /**
         * End iterator.
         */
        iterator() = default;
        /**
         * Start at the given type and probe it immediately.
         *
         * @param type   the first device type to probe
         * @param device backend-specific selector, borrowed for the whole iteration
         */
        iterator(DeviceType type, const char* device)
            : m_type(type), m_device(device)
        {
            probe();
        }

        /**
         * Result of the current probe.
         *
         * @return the probe of the type the iterator points at
         */
        reference operator*() const noexcept { return m_probe; }
        /**
         * Result of the current probe.
         *
         * @return a pointer to the probe result
         */
        pointer operator->() const noexcept { return &m_probe; }

        /**
         * Move to the next type and probe it.
         *
         * @return this iterator
         */
        iterator& operator++() noexcept
        {
            if (!m_type.isNone())
                m_type = DeviceType(av_hwdevice_iterate_types(m_type.get()));
            probe();
            return *this;
        }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when both point at the same type
         */
        bool operator==(const iterator& other) const noexcept { return m_type.get() == other.m_type.get(); }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when the iterators point at different types
         */
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

    /**
     * Empty range, used as the past-the-end value.
     */
    DeviceProbeRange() = default;
    /**
     * Range over all types with a borrowed device selector.
     *
     * @param device backend-specific selector; must outlive the range
     */
    explicit DeviceProbeRange(const char* device) noexcept
        : m_device(device)
    {}

    /**
     * Range over all types with a device selector stored by the range.
     *
     * @param device backend-specific selector; a copy is kept
     */
    explicit DeviceProbeRange(const std::string& device)
        : m_owned(device), m_device(m_owned->c_str())
    {}

    /**
     * Copy the range, keeping the stored selector valid.
     *
     * @param other the range to copy
     */
    DeviceProbeRange(const DeviceProbeRange& other)
        : m_owned(other.m_owned),
          m_device(other.m_owned ? m_owned->c_str() : other.m_device)
    {}

    /**
     * Move the range.
     *
     * @param other the range to move from; left empty
     */
    DeviceProbeRange(DeviceProbeRange&& other) noexcept
        : m_owned(std::move(other.m_owned)),
          m_device(m_owned ? m_owned->c_str() : other.m_device)
    {
        other.m_device = nullptr;
    }

    /**
     * Assign through copy-and-swap.
     *
     * @param other the range to assign from
     *
     * @return this range
     */
    DeviceProbeRange& operator=(DeviceProbeRange other)
    {
        swap(other);
        return *this;
    }

    /**
     * Exchange the state of two ranges, fixing up the stored selectors.
     *
     * @param other the range to swap with
     */
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

    /**
     * Start probing.
     *
     * @return an iterator at the first compiled type, already probed
     */
    iterator begin() const
    {
        return iterator(DeviceType(av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE)), m_device);
    }
    /**
     * End of the range.
     *
     * @return the past-the-end iterator
     */
    iterator end() const { return iterator(); }

private:
    std::optional<std::string> m_owned;
    const char*                m_device = nullptr;
};

/**
 * Probe every compiled device type with a borrowed selector.
 *
 * Each step creates a device and destroys it immediately, which is the only
 * reliable availability check: FFmpeg lists compiled backends, not hardware.
 *
 * @param device backend-specific selector, for example "/dev/dri/renderD128"
 *               or "1"; nullptr asks for the default device
 *
 * @return a lazy range of results; the selector must outlive the range
 */
DeviceProbeRange probeDevices(const char* device = nullptr);
/**
 * Probe every compiled device type, keeping a copy of the selector.
 *
 * @param device backend-specific selector; a copy is stored in the range
 *
 * @return a lazy range of results
 */
DeviceProbeRange probeDevices(const std::string& device);

/**
 * Check whether one device type can be created.
 *
 * @param type   the device type to probe
 * @param device backend-specific selector; nullptr asks for the default
 *
 * @return `true` when FFmpeg managed to create a device; the temporary
 *         device is destroyed before the call returns
 */
bool isDeviceAvailable(DeviceType type, const char* device = nullptr);
/**
 * Check whether one device type can be created.
 *
 * @param type   the device type to probe
 * @param device backend-specific selector
 *
 * @return `true` when FFmpeg managed to create a device
 */
bool isDeviceAvailable(DeviceType type, const std::string& device);

/**
 * @brief Non-owning view over a null-terminated AVPixelFormat array.
 */
class PixelFormatRange
{
public:
    /**
     * Forward iterator over a null-terminated AVPixelFormat array.
     */
    class iterator
    {
    public:
        /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
        /// Type produced by dereferencing.
        using value_type        = PixelFormat;
        /// Distance type required by the iterator traits.
        using difference_type   = std::ptrdiff_t;
        /// No pointer-to-element type: the value is produced on dereference.
        using pointer           = void;
        /// Dereference result type.
        using reference         = PixelFormat;

        /**
         * End iterator.
         */
        iterator() = default;
        /**
         * Point the iterator at an array element.
         *
         * @param ptr position in the array
         */
        explicit iterator(const AVPixelFormat* ptr) noexcept : m_ptr(ptr) {}

        /**
         * Format at the current position.
         *
         * @return the wrapped format
         */
        PixelFormat operator*() const noexcept { return PixelFormat(*m_ptr); }
        /**
         * Advance to the next format.
         *
         * @return this iterator
         */
        iterator& operator++() noexcept
        {
            ++m_ptr;
            return *this;
        }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when both point at the same element
         */
        bool operator==(const iterator& other) const noexcept { return m_ptr == other.m_ptr; }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when the iterators point at different elements
         */
        bool operator!=(const iterator& other) const noexcept { return m_ptr != other.m_ptr; }

    private:
        const AVPixelFormat* m_ptr = nullptr;
    };

    /**
     * Create an empty range.
     */
    PixelFormatRange() = default;
    /**
     * Wrap a null-terminated format array.
     *
     * @param formats pointer to the first element, terminated by
     *                `AV_PIX_FMT_NONE`; may be null
     *
     * @note Non-owning: the array must outlive the range.
     */
    explicit PixelFormatRange(const AVPixelFormat* formats) noexcept;

    /**
     * First format.
     *
     * @return an iterator at the first element
     */
    iterator begin() const noexcept { return iterator(m_begin); }
    /**
     * One past the last format.
     *
     * @return the past-the-end iterator
     */
    iterator end() const noexcept { return iterator(m_end); }
    /**
     * Number of formats.
     *
     * @return the element count
     */
    size_t size() const noexcept { return static_cast<size_t>(m_end - m_begin); }
    /**
     * Whether the range has no formats.
     *
     * @return `true` when the range is empty
     */
    bool empty() const noexcept { return m_begin == m_end; }
    /**
     * Format access.
     *
     * @param index zero-based index; must be less than size()
     *
     * @return the wrapped format at the index
     */
    PixelFormat operator[](size_t index) const noexcept { return PixelFormat(m_begin[index]); }

    /**
     * Copy the range into a vector.
     *
     * @return a vector with the same formats
     */
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
    /// Iterator type of the owned array.
    using iterator = PixelFormatRange::iterator;

    /**
     * Create an empty list.
     */
    PixelFormatList() = default;
    /**
     * Take ownership of a format array allocated by FFmpeg.
     *
     * @param formats array terminated by `AV_PIX_FMT_NONE`, as returned by
     *                `av_hwframe_transfer_get_formats()` and friends; may be null
     *
     * @note The array is freed with `av_freep()` in the destructor. Do not free
     *       it yourself and do not pass an array you do not own.
     */
    explicit PixelFormatList(AVPixelFormat* formats) noexcept
        : m_formats(formats)
    {}
    /**
     * Free the owned array.
     */
    ~PixelFormatList();

    /**
     * Move the array out of another list.
     *
     * @param other the list to move from; left empty
     */
    PixelFormatList(PixelFormatList&& other) noexcept
        : m_formats(other.m_formats)
    {
        other.m_formats = nullptr;
    }
    /**
     * Move assignment: frees the current array and takes the other one.
     *
     * @param other the list to move from; left empty
     *
     * @return this list
     */
    PixelFormatList& operator=(PixelFormatList&& other) noexcept;

    /**
     * Copying is disabled: the list owns its array.
     */
    PixelFormatList(const PixelFormatList&) = delete;
    /**
     * Copy assignment is disabled: the list owns its array.
     */
    PixelFormatList& operator=(const PixelFormatList&) = delete;

    /**
     * Non-owning view over the array.
     *
     * @return a view that is valid while this list is alive
     */
    PixelFormatRange range() const noexcept { return PixelFormatRange(m_formats); }
    /**
     * First format.
     *
     * @return an iterator at the first element
     */
    iterator begin() const noexcept { return range().begin(); }
    /**
     * One past the last format.
     *
     * @return the past-the-end iterator
     */
    iterator end() const noexcept { return range().end(); }
    /**
     * Number of formats.
     *
     * @return the element count
     */
    size_t size() const noexcept { return range().size(); }
    /**
     * Whether the list has no formats.
     *
     * @return `true` when the list is empty
     */
    bool empty() const noexcept { return range().empty(); }
    /**
     * Copy the list into a vector.
     *
     * @return a vector with the same formats
     */
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
    /**
     * Create an empty value; `isValid()` is false.
     */
    FrameConstraints() = default;

    /**
     * Ask the device for its frame constraints.
     *
     * @param device the device to query
     * @param ec     holder for the error code
     *
     * @return an owning value; invalid when the device reports no constraints
     *
     * @note Some backends report very wide bounds, from 0x0 to INT_MAX. Treat the
     *       numbers as allowed ranges, not as the capabilities of the silicon.
     */
    static FrameConstraints query(const DeviceContext& device,
                                  OptionalErrorCode ec = throws());

    /**
     * Whether constraints were obtained.
     *
     * @return `true` when the value owns a constraints structure
     */
    bool isValid() const noexcept { return static_cast<bool>(m_raw); }
    /**
     * Hardware surface formats the device supports.
     *
     * @return a non-owning view into the owned structure
     */
    PixelFormatRange hwFormats() const noexcept;
    /**
     * System-memory formats that can be transferred to the device.
     *
     * @return a non-owning view into the owned structure
     */
    PixelFormatRange swFormats() const noexcept;
    /**
     * Smallest supported width.
     *
     * @return the minimum width in pixels
     */
    int minWidth() const noexcept;
    /**
     * Largest supported width.
     *
     * @return the maximum width in pixels
     */
    int maxWidth() const noexcept;
    /**
     * Smallest supported height.
     *
     * @return the minimum height in pixels
     */
    int minHeight() const noexcept;
    /**
     * Largest supported height.
     *
     * @return the maximum height in pixels
     */
    int maxHeight() const noexcept;

private:
    struct Deleter
    {
        void operator()(AVHWFramesConstraints* ptr) const noexcept;
    };

    std::unique_ptr<AVHWFramesConstraints, Deleter> m_raw;
};

/**
 * Query hardware frames constraints for a device.
 *
 * The returned object owns the FFmpeg constraints; hwFormats() and swFormats()
 * are non-owning views into it.
 *
 * @param device the device to query
 * @param ec     holder for the error code
 *
 * @return an owning value; check isValid() before reading it
 */
FrameConstraints frameConstraints(const DeviceContext& device,
                                  OptionalErrorCode ec = throws());

/**
 * @brief Non-owning iterator over AVCodecHWConfig entries of a codec.
 */
class CodecConfigRange
{
public:
    /**
     * Forward iterator over the hardware configurations of a codec.
     */
    class iterator
    {
    public:
        /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
        /// Type produced by dereferencing.
        using value_type        = AVCodecHWConfig;
        /// Distance type required by the iterator traits.
        using difference_type   = std::ptrdiff_t;
        /// Pointer to the configuration entry.
        using pointer           = const AVCodecHWConfig*;
        /// Reference to the configuration entry.
        using reference         = const AVCodecHWConfig&;

        /**
         * End iterator.
         */
        iterator() = default;
        /**
         * Point the iterator at a configuration index.
         *
         * @param codec the codec to inspect
         * @param index zero-based configuration index; negative means end
         */
        iterator(const AVCodec* codec, int index) noexcept
            : m_codec(codec), m_index(index)
        {
            refresh();
        }

        /**
         * Configuration at the current position.
         *
         * @return the FFmpeg configuration entry
         */
        reference operator*() const noexcept { return m_value; }
        /**
         * Configuration at the current position.
         *
         * @return a pointer to the FFmpeg configuration entry
         */
        pointer operator->() const noexcept { return &m_value; }

        /**
         * Advance to the next configuration.
         *
         * @return this iterator
         */
        iterator& operator++() noexcept
        {
            ++m_index;
            refresh();
            return *this;
        }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when both point at the same configuration
         */
        bool operator==(const iterator& other) const noexcept
        {
            if (m_codec != other.m_codec)
                return false;
            if (!m_valid || !other.m_valid)
                return m_valid == other.m_valid;
            return m_index == other.m_index;
        }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when the iterators point at different configurations
         */
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

    /**
     * Create an empty range.
     */
    CodecConfigRange() = default;
    /**
     * Iterate the hardware configurations of a codec.
     *
     * @param codec the codec to inspect; the range is empty when it is not valid
     */
    explicit CodecConfigRange(const Codec& codec) noexcept
        : m_codec(codec.raw())
    {}

    /**
     * First configuration.
     *
     * @return an iterator at index zero
     */
    iterator begin() const { return iterator(m_codec, 0); }
    /**
     * End of the range.
     *
     * @return the past-the-end iterator
     */
    iterator end() const { return iterator(m_codec, -1); }

    /**
     * Whether the codec has no hardware configuration.
     *
     * @return `true` when there is nothing to iterate
     */
    bool empty() const { return begin() == end(); }

private:
    const AVCodec* m_codec = nullptr;
};

/**
 * All hardware configurations of a codec.
 *
 * @param codec the codec to inspect
 *
 * @return a non-owning range over the configurations FFmpeg reports
 */
CodecConfigRange codecConfigs(const Codec& codec) noexcept;

/**
 * The first hardware configuration that matches a device type.
 *
 * @param codec       the codec to inspect
 * @param type        the device type to match
 * @param methodsMask configuration methods to accept; by default both the
 *                    device-context and the frames-context methods qualify
 *
 * @return the matching entry, or `std::nullopt` when the codec cannot use
 *         this device type with the accepted methods
 */
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
    /**
     * Forward iterator over the codecs FFmpeg was built with.
     */
    class iterator
    {
    public:
        /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
        /// Type produced by dereferencing.
        using value_type        = Codec;
        /// Distance type required by the iterator traits.
        using difference_type   = std::ptrdiff_t;
        /// No pointer-to-element type: the value is produced on dereference.
        using pointer           = void;
        /// Dereference result type.
        using reference         = Codec;

        /**
         * End iterator.
         */
        iterator() = default;
        /**
         * Start the iteration and skip everything that does not match.
         *
         * @param id       the codec identifier to accept, or `AV_CODEC_ID_NONE` for any
         * @param type     the device type the codec must support
         * @param encoders `true` to look for encoders, `false` for decoders
         */
        iterator(AVCodecID id, DeviceType type, bool encoders)
            : m_id(id), m_type(type), m_encoders(encoders)
        {
            advance();
        }

        /**
         * Codec at the current position.
         *
         * @return a wrapper around the FFmpeg codec
         */
        Codec operator*() const noexcept { return Codec(m_codec); }
        /**
         * Advance to the next matching codec.
         *
         * @return this iterator
         */
        iterator& operator++() noexcept
        {
            advance();
            return *this;
        }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when both point at the same codec
         */
        bool operator==(const iterator& other) const noexcept { return m_codec == other.m_codec; }
        /**
         * Compare positions.
         *
         * @param other the iterator to compare with
         *
         * @return `true` when the iterators point at different codecs
         */
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

    /**
     * Create an empty range.
     */
    CodecRange() = default;
    /**
     * Describe the codecs to iterate.
     *
     * @param id       codec identifier filter, or `AV_CODEC_ID_NONE` for any
     * @param type     device type the codec must support
     * @param encoders `true` for encoders, `false` for decoders
     */
    CodecRange(AVCodecID id, DeviceType type, bool encoders) noexcept
        : m_id(id), m_type(type), m_encoders(encoders)
    {}

    /**
     * First matching codec.
     *
     * @return an iterator at the first match
     */
    iterator begin() const { return iterator(m_id, m_type, m_encoders); }
    /**
     * End of the range.
     *
     * @return the past-the-end iterator
     */
    iterator end() const { return iterator(); }

private:
    AVCodecID  m_id = AV_CODEC_ID_NONE;
    DeviceType m_type;
    bool       m_encoders = false;
};

/**
 * All decoders for a codec identifier that support a device type.
 *
 * The search covers every codec with the identifier, which matters for QSV:
 * the generic `h264` decoder cannot use QSV, `h264_qsv` can.
 *
 * @param id   the codec identifier
 * @param type the device type the decoder must support
 *
 * @return a lazy, non-owning range
 */
CodecRange findDecoders(AVCodecID id, DeviceType type) noexcept;
/**
 * All encoders for a codec identifier that support a device type.
 *
 * @param id   the codec identifier
 * @param type the device type the encoder must support
 *
 * @return a lazy, non-owning range
 */
CodecRange findEncoders(AVCodecID id, DeviceType type) noexcept;
/**
 * The first decoder for a codec identifier that supports a device type.
 *
 * @param id   the codec identifier
 * @param type the device type the decoder must support
 *
 * @return a valid codec, or an invalid one when nothing matches
 */
Codec findDecoder(AVCodecID id, DeviceType type) noexcept;
/**
 * The first encoder for a codec identifier that supports a device type.
 *
 * @param id   the codec identifier
 * @param type the device type the encoder must support
 *
 * @return a valid codec, or an invalid one when nothing matches
 */
Codec findEncoder(AVCodecID id, DeviceType type) noexcept;

/**
 * Hardware pixel format used by a device type.
 *
 * The lookup is name based, so the function does not depend on enum values
 * that may be missing in older FFmpeg versions.
 *
 * @param type the device type
 *
 * @return the surface format, for example `AV_PIX_FMT_VAAPI`, or
 *         `AV_PIX_FMT_NONE` when the type is unknown
 */
PixelFormat hwPixelFormat(DeviceType type) noexcept;

} // namespace hw
} // namespace av
