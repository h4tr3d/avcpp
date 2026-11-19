#pragma once

/**
 * Video frames, audio samples and their common frame interface.
 *
 * @file
 * @ingroup frame
 */

#include <vector>

#include "avcompat.h"
#include "avcpp/buffer.h"
#include "avcpp/dictionary.h"

#if AVCPP_CXX_STANDARD >= 20
#if __has_include(<span>)
#include <span>
#endif
#endif

#include "ffmpeg.h"
#include "hwcontext.h"
#include "rational.h"
#include "timestamp.h"
#include "pixelformat.h"
#include "sampleformat.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/attributes.h>
}

namespace av
{

namespace frame
{
/**
 * Timestamp FFmpeg considers the best estimate of the presentation time.
 *
 * Falls back from `pts` to `best_effort_timestamp` and then to `pkt_dts`, in
 * the order FFmpeg itself uses.
 *
 * @param frame the frame to inspect
 *
 * @return the timestamp, or `AV_NOPTS_VALUE` when the frame carries none
 */
int64_t get_best_effort_timestamp(const AVFrame* frame);
/**
 * Channel layout of a frame as a bit mask.
 *
 * Bridges the old bit-mask layout and the AVChannelLayout API, so callers do
 * not have to care which one the FFmpeg build uses.
 *
 * @param frame the frame to inspect
 *
 * @return an `AV_CH_LAYOUT_*` mask, or zero when the frame has none
 */
uint64_t get_channel_layout(const AVFrame* frame);
/**
 * Set the channel layout of a frame from a bit mask.
 *
 * @param frame  the frame to modify; must not be null
 * @param layout an `AV_CH_LAYOUT_*` mask
 */
void set_channel_layout(AVFrame* frame, uint64_t layout);
} // ::av::frame

#if AVCPP_HAS_FRAME_SIDE_DATA
/**
 * Simple view for the AVFrameSideData elements
 */
class FrameSideData : public FFWrapperPtr<AVFrameSideData>
{
public:
    using FFWrapperPtr<AVFrameSideData>::FFWrapperPtr;

    /**
     * Name of the side data type, as FFmpeg spells it.
     *
     * @return a view into a static string owned by FFmpeg
     */
    std::string_view name() const noexcept;
    /**
     * Identifier of the side data type.
     *
     * @return the `AV_FRAME_DATA_*` value
     */
    AVFrameSideDataType type() const noexcept;
    /**
     * Read-only view of the payload.
     *
     * @return the bytes of this side data entry
     */
    std::span<const uint8_t> span() const noexcept;
    /**
     * Mutable view of the payload.
     *
     * @return the bytes of this side data entry
     */
    std::span<uint8_t> span() noexcept;

    /**
     * The buffer that owns the payload.
     *
     * @return a non-owning view; empty when the entry has no buffer
     */
    BufferRefView buffer() const noexcept;

    /**
     * @brief metadata
     * @return  AVFrameSideData::metadata view. Owning is not taken
     */
    Dictionary metadata() const noexcept;

    /**
     * Name of a side data type.
     *
     * @param type the `AV_FRAME_DATA_*` value
     *
     * @return a view into a static string owned by FFmpeg, or an empty view for an
     *         unknown type
     */
    static std::string_view name(AVFrameSideDataType type) noexcept;

#if AVCPP_API_HAS_AVSIDEDATADESCRIPTOR
    /**
     * Descriptor of this side data type, when the FFmpeg build provides one.
     *
     * @return the descriptor, or `std::nullopt` for an unknown type
     */
    std::optional<AVSideDataDescriptor> descriptor() const noexcept;
    /**
     * Descriptor of a side data type, when the FFmpeg build provides one.
     *
     * @param type the `AV_FRAME_DATA_*` value
     *
     * @return the descriptor, or `std::nullopt` for an unknown type
     */
    static std::optional<AVSideDataDescriptor> descriptor(AVFrameSideDataType type) noexcept;
#endif // AVCPP_API_HAS_AVSIDEDATADESCRIPTOR

    /**
     * Whether the entry carries no payload.
     *
     * @return `true` when the payload size is zero
     */
    bool empty() const noexcept;
    /**
     * Whether the view refers to a real entry.
     *
     * @return `true` when the entry is present and its payload is not empty
     */
    operator bool() const noexcept;
};
#endif // AVCPP_HAS_PKT_SIDE_DATA

/**
 * Common interface of video frames and audio samples.
 *
 * Owns an AVFrame and adds the avcpp bookkeeping that FFmpeg keeps outside it:
 * the time base, the stream index and a completeness flag. Derived classes are
 * expected to implement assignment through assignOperator()/moveOperator();
 * direct assignment is deleted here to make slicing impossible.
 *
 * Reference counting follows AVPacket/AVFrame semantics: a copy of the wrapper
 * references the same frame buffer, and clone() is the way to get an
 * independent copy.
 */
class FrameCommon : public FFWrapperPtr<AVFrame>
{
public:
    /**
     * Wrap data and take owning. Data must be allocated with av_malloc()/av::malloc() family
     */
    struct wrap_data {};
    /**
     * Wrap static data, do not owning and free.
     * Buffer size must be: size + AV_INPUT_BUFFER_PADDING_SIZE
     */
    struct wrap_data_static {};

    /**
     * Create an invalid frame.
     */
    FrameCommon();
    /**
     * Copy the frame metadata into a new wrapper.
     *
     * The data itself is referenced, not copied.
     *
     * @param frame the frame to reference
     */
    FrameCommon(const AVFrame *frame);

    // Helper ctors to implement move/copy ctors
    /**
     * Copy constructor: references the same buffer.
     *
     * @param other the wrapper to copy
     */
    FrameCommon(const FrameCommon& other);
    /**
     * Move constructor.
     *
     * @param other the wrapper to move from; left invalid
     */
    FrameCommon(FrameCommon&& other);

    ~FrameCommon();

    /**
     * Whether the frame data is reference counted.
     *
     * @return `true` when the buffer is shared through AVBufferRef
     */
    bool isReferenced() const;
    /**
     * Number of references to the frame buffer.
     *
     * @return the reference count, or zero when the frame is not referenced
     */
    int refCount() const;
    /**
     * Create an independent reference to the frame.
     *
     * @return a new AVFrame that shares the buffer and must be freed by the
     *         caller, or nullptr when the frame is invalid
     */
    AVFrame* makeRef() const;

    /**
     * Presentation timestamp.
     *
     * @return the timestamp in this frame's time base
     */
    Timestamp pts() const;
    /**
     * Set the timestamp and its time base at once.
     *
     * @deprecated Pass a Timestamp instead, so the time base travels with the
     *             value and cannot be lost.
     *
     * @param pts           timestamp in `ptsTimeBase` units
     * @param ptsTimeBase   time base of the timestamp
     */
    attribute_deprecated void setPts(int64_t pts, Rational ptsTimeBase);
    /**
     * Set the timestamp.
     *
     * @param ts the timestamp; its time base is stored with the frame
     */
    void setPts(const Timestamp &ts);

    /**
     * Time base of the frame.
     *
     * @return timestamp units per second
     */
    const Rational& timeBase() const;
    /**
     * Set the time base.
     *
     * @param value timestamp units per second; set it before the frame is
     *              passed to another component
     */
    void setTimeBase(const Rational &value);

    /**
     * Index of the stream the frame belongs to.
     *
     * @return the index, or -1 when it has not been set
     */
    int streamIndex() const;
    /**
     * Set the stream index.
     *
     * @param streamIndex index of the stream in the container
     */
    void setStreamIndex(int streamIndex);

    /**
     * Mark the frame as complete or incomplete.
     *
     * @param isComplete `true` when the frame holds all the data it should
     */
    void setComplete(bool isComplete);
    /**
     * Whether the frame is marked complete.
     *
     * @return the completeness flag; settable with setComplete()
     */
    bool isComplete() const;

    /**
     * Whether the wrapper holds a frame.
     *
     * @return `true` when the underlying AVFrame exists
     */
    bool isValid() const;

    /**
     * Whether the wrapper holds a frame.
     *
     * @return `true` when the underlying AVFrame exists
     */
    operator bool() const;

    /**
     * Whether the frame lives in device memory.
     *
     * @return `true` when `hw_frames_ctx` is set, which means `data[]` is not
     *         directly readable
     */
    bool isHwFrame() const;

    /**
     * Pointer to one image or audio plane.
     *
     * @param plane zero-based plane index; audio uses one plane per channel
     *              for planar formats
     *
     * @return the plane pointer, or nullptr when it is absent
     *
     * @note Borrowed. The pointer is valid while the frame's buffer lives.
     */
    uint8_t *data(size_t plane = 0);
    /**
     * Pointer to one plane, read-only.
     *
     * @param plane zero-based plane index
     *
     * @return the plane pointer, or nullptr when it is absent
     */
    const uint8_t *data(size_t plane = 0) const;

    /**
     * Size of one plane.
     *
     * @param plane zero-based plane index
     *
     * @return the size in bytes
     */
    size_t size(size_t plane) const;
    /**
     * Total size of all planes.
     *
     * @return the sum of the plane sizes in bytes
     */
    size_t size() const;

    /**
     * Print the frame layout through the FFmpeg log system.
     */
    void dump() const;

    // You must implement operators in deveritive classes using assignOperator() and moveOperator()
    /**
     * Assignment is deleted: derived classes implement it to avoid slicing.
     */
    void operator=(const FrameCommon&) = delete;

    /**
     * Exchange the contents of two frames.
     *
     * @param other the frame to swap with
     */
    void swap(FrameCommon &other);

#if AVCPP_HAS_FRAME_SIDE_DATA
    /**
     * Get packet side data of the given type. Empty buffer means no data.
     *
     * @param type
     * @return
     */
    const FrameSideData sideData(AVFrameSideDataType type) const;
    /**
     * Mutable access to the side data of the given type.
     *
     * @param type the `AV_FRAME_DATA_*` value to look for
     *
     * @return a view of the entry, empty when the frame has none
     */
    FrameSideData       sideData(AVFrameSideDataType type);

    /**
     * Return count of the side data elements
     * @return
     */
    std::size_t sideDataCount() const noexcept;

    /**
     * Get side data element by index
     * @param index
     * @return
     */
    FrameSideData sideDataIndex(std::size_t index) noexcept;

    /**
     * Observe all packet side data via iterators
     *
     * Applicable for range-based for and std::range:xxx algos.
     *
     * @return
     */
    ArrayView<AVFrameSideData*, FrameSideData, std::size_t> sideData() noexcept;
    /**
     * Read-only iteration over all side data entries.
     *
     * @return a non-owning range valid while the frame is alive
     */
    ArrayView<const AVFrameSideData*, FrameSideData, std::size_t> sideData() const noexcept;


    /**
     * Remove and free all side data instances of the given type.
     * @param type
     */
    void sideDataRemove(AVFrameSideDataType type) noexcept;

    /**
     * Add side data of the given type into packet. Data will be cloned.
     * @param type
     * @param data
     * @param ec
     * @return a view of the added entry
     */
    FrameSideData addSideData(AVFrameSideDataType type, std::span<const uint8_t> data, OptionalErrorCode ec = throws());

    /**
     * Add side data of the given type into packet. Data will be wrapped and owner-shipping taken. Data must be allocated
     * via av_malloc()/av::malloc() family functions.
     *
     * @param type
     * @param data
     * @param ec
     * @return a view of the added entry
     */
    FrameSideData addSideData(AVFrameSideDataType type, std::span<uint8_t> data, wrap_data, OptionalErrorCode ec = throws());

    /**
     * Add side data of the given type into packet. Data will be wrapped without copying and null deleter will be provided.
     * So data maybe any static data.
     *
     * @param type
     * @param data
     * @param ec
     * @return a view of the added entry
     */
    FrameSideData addSideData(AVFrameSideDataType type, std::span<uint8_t> data, wrap_data_static, OptionalErrorCode ec = throws());

    /**
     * Add side data of the given type into packet. Setup via buffer reference view. Nested API does not reference
     * buffer so it must be done with the caller code.
     * @param type
     * @param buf
     * @param ec
     * @return a view of the added entry
     */
    FrameSideData addSideData(AVFrameSideDataType type, BufferRef buf, OptionalErrorCode ec = throws());

    /**
     * Allocate storage for the packet side data of the given type and return reference to it. Data owned by the packet.
     * @param type
     * @param size
     * @param ec
     * @return
     */
    FrameSideData allocateSideData(AVFrameSideDataType type, std::size_t size, OptionalErrorCode ec = throws());
#endif

protected:
    /**
     * Copy the avcpp bookkeeping (time base, stream index, completeness).
     *
     * @param other the frame to copy from
     */
    void copyInfoFrom(const FrameCommon& other);
    /**
     * Make a deep copy of the frame data into another frame object.
     *
     * @param dst   the frame that receives the copy
     * @param align alignment in bytes for the allocated planes
     */
    void clone(FrameCommon& dst, size_t align = 1) const;

protected:
        /// Time base of the timestamps in this frame.
    Rational             m_timeBase{};
        /// Index of the stream the frame belongs to; -1 when unset.
    int                  m_streamIndex {-1};
        /// Completeness flag, set through setComplete().
    bool                 m_isComplete  {false};
};

/**
 * CRTP base that gives a frame type its copy and move assignment.
 *
 * The derived class passes itself as `T`; the base supplies the operations
 * that must not be inherited from FrameCommon directly.
 *
 * @tparam T the derived frame type
 */
template<typename T>
class Frame : public FrameCommon
{
protected:
    /**
     * Copy the other frame into this one.
     *
     * @param rhs the frame to copy
     *
     * @return this frame
     */
    T& assignOperator(const T &rhs) {
        if (this == &rhs)
            return static_cast<T&>(*this);
        T(rhs).swap(static_cast<T&>(*this));
        return static_cast<T&>(*this);
    }

    /**
     * Move the other frame into this one.
     *
     * @param rhs the frame to move from
     *
     * @return this frame
     */
    T& moveOperator(T &&rhs) {
        if (this == &rhs)
            return static_cast<T&>(*this);
        T(std::move(rhs)).swap(static_cast<T&>(*this));
        return static_cast<T&>(*this);
    }

public:
    using FrameCommon::FrameCommon;

    /**
     * An invalid frame of this type.
     *
     * @return a frame wrapper that holds no AVFrame
     */
    static T null() {
        return T(nullptr);
    }

    /**
     * Deep copy of the frame.
     *
     * @param align alignment in bytes for the allocated planes
     *
     * @return an independent frame; invalid when this one is invalid
     */
    T clone(size_t align = 1) const {
        T result;
        FrameCommon::clone(result, align);
        return result;
    }
};

static_assert(std::is_copy_assignable_v<FrameCommon> == false);
static_assert(std::is_copy_constructible_v<FrameCommon> == true);
static_assert(std::is_move_assignable_v<FrameCommon> == false);
static_assert(std::is_move_constructible_v<FrameCommon> == true);

/**
 * One video frame.
 *
 * The frame can live in system memory, where `data[]` is readable, or in device
 * memory, where it is only a descriptor for a hardware surface; isHwFrame()
 * tells the two apart and toSystemMemory() moves between them.
 */
class VideoFrame : public Frame<VideoFrame>
{
public:
    using Frame<VideoFrame>::Frame;

    /**
     * Create an invalid frame.
     */
    VideoFrame() = default;
    /**
     * Allocate an empty frame for the given format and size.
     *
     * @param pixelFormat pixel format of the frame
     * @param width       width in pixels
     * @param height      height in pixels
     * @param align       alignment in bytes for each plane
     */
    VideoFrame(PixelFormat pixelFormat, int width, int height, int align = 1);
    /**
     * Copy external image data into a frame.
     *
     * @param data        buffer holding the image
     * @param size        size of the buffer in bytes
     * @param pixelFormat pixel format of the data
     * @param width       width in pixels
     * @param height      height in pixels
     * @param align       alignment in bytes for each plane
     */
    VideoFrame(const uint8_t *data, size_t size, PixelFormat pixelFormat, int width, int height, int align = 1);

    /**
     * Copy constructor: references the same frame buffer.
     *
     * @param other the frame to copy
     */
    VideoFrame(const VideoFrame &other);
    /**
     * Move constructor.
     *
     * @param other the frame to move from; left invalid
     */
    VideoFrame(VideoFrame &&other);

    /**
     * Copy assignment: references the buffer of the other frame.
     *
     * @param rhs the frame to assign from
     *
     * @return this frame
     */
    VideoFrame& operator=(const VideoFrame &rhs);
    /**
     * Move assignment.
     *
     * @param rhs the frame to move from; left invalid
     *
     * @return this frame
     */
    VideoFrame& operator=(VideoFrame &&rhs);

    /**
     * Pixel format of the frame.
     *
     * @return the format, or `AV_PIX_FMT_NONE` for an invalid frame
     */
    PixelFormat            pixelFormat() const;
    /**
     * Frame width.
     *
     * @return the width in pixels, or zero for an invalid frame
     */
    int                    width() const;
    /**
     * Frame height.
     *
     * @return the height in pixels, or zero for an invalid frame
     */
    int                    height() const;

    /**
     * Convert JPEG pixel format to the compatible non-JPEG one
     *
     * YUV Pixel formats with J in the naming (AV_PIX_FMT_YUVJxxx) mostly deprecated and some components warn about it.
     * Allow to adjust internal pixel format into compatible one.
     *
     * @return previous pixel format
     */
    PixelFormat            adjustFromJpegPixelFormat();
    /**
     * Convert non-JPEG pixel format to the compatible JPEG one
     *
     * YUV Pixel formats with J in the naming (AV_PIX_FMT_YUVJxxx) mostly deprecated and some components warn about it.
     * Allow to adjust internal pixel format into compatible one.
     *
     * @return previous pixel format
     */
    PixelFormat            adjustToJpegPixelFormat();

    /**
     * Whether the frame is a key frame.
     *
     * @return the `AV_FRAME_FLAG_KEY` state
     */
    bool                   isKeyFrame() const;
    /**
     * Mark the frame as a key frame.
     *
     * @param isKey the new state of the key-frame flag
     */
    void                   setKeyFrame(bool isKey);

    /**
     * Quality value attached to the frame.
     *
     * @return the quality in the `FF_LAMBDA_*` scale
     */
    int                    quality() const;
    /**
     * Attach a quality value to the frame.
     *
     * @param quality quality in the `FF_LAMBDA_*` scale
     */
    void                   setQuality(int quality);

    /**
     * Picture type assigned by the decoder or expected by the encoder.
     *
     * @return the `AV_PICTURE_TYPE_*` value
     */
    AVPictureType          pictureType() const;
    /**
     * Set the picture type.
     *
     * @param type the `AV_PICTURE_TYPE_*` value; the default lets the encoder
     *             decide
     */
    void                   setPictureType(AVPictureType type = AV_PICTURE_TYPE_NONE);

    /**
     * Pixel aspect ratio of the frame.
     *
     * @return the ratio, or an empty value when it is not set
     */
    Rational               sampleAspectRatio() const;
    /**
     * Set the pixel aspect ratio.
     *
     * @param sampleAspectRatio width divided by height of a single pixel
     */
    void                   setSampleAspectRatio(const Rational& sampleAspectRatio);

    /**
     * Size of the buffer needed to hold the frame in one contiguous block.
     *
     * @param align alignment in bytes for each plane
     * @param ec    holder for the error code
     *
     * @return the required size in bytes
     */
    size_t                 bufferSize(int align = 1, OptionalErrorCode ec = throws()) const;
    /**
     * Copy the image into a caller-provided buffer.
     *
     * @param dst   destination buffer
     * @param size  size of the destination in bytes
     * @param align alignment in bytes for each plane
     * @param ec    holder for the error code
     *
     * @return `true` when the frame was copied
     */
    bool                   copyToBuffer(uint8_t *dst, size_t size, int align = 1, OptionalErrorCode ec = throws());
    /**
     * Copy the image into a vector, resizing it as needed.
     *
     * @param dst   destination vector; resized to bufferSize()
     * @param align alignment in bytes for each plane
     * @param ec    holder for the error code
     *
     * @return `true` when the frame was copied
     */
    bool                   copyToBuffer(std::vector<uint8_t>& dst, int align = 1, OptionalErrorCode ec = throws());

    /**
     * @brief Deep copy of a software frame.
     *
     * Hardware frames cannot be copied implicitly: the function reports
     * Errors::HardwareTransferUnsupported. Use toSystemMemory() to transfer a
     * hardware frame into system memory first. An invalid frame produces an
     * invalid copy without an error.
     * @param align
     * @param ec
     * @return an independent frame, or an invalid frame when this one is invalid
     */
    VideoFrame clone(size_t align = 1, OptionalErrorCode ec = throws()) const;

    /**
     * @brief Transfer a hardware frame into system memory.
     *
     * The function allocates a software frame and copies the hardware surface
     * into it. When @p format is not AV_PIX_FMT_NONE, the transfer requests
     * that format. The format must be supported by the hardware backend; see
     * FramesContext::transferFormats().
     *
     * A software frame is returned as a reference copy when no format
     * conversion is requested.
     * @param format
     * @param ec
     * @return a frame in system memory
     */
    VideoFrame toSystemMemory(PixelFormat format = AV_PIX_FMT_NONE,
                              OptionalErrorCode ec = throws()) const;

    /**
     * @brief Upload a software frame into hardware memory.
     *
     * The destination buffer is allocated with av_hwframe_get_buffer() and then
     * filled with av_hwframe_transfer_data(). A hardware frame that already
     * belongs to @p frames is returned as a reference copy.
     * @param frames
     * @param ec
     * @return a frame in the given hardware pool
     */
    VideoFrame toHwFrame(const hw::FramesContext& frames,
                         OptionalErrorCode ec = throws()) const;

    /**
     * @brief Map this hardware frame into another hardware frames context.
     *
     * Uses av_hwframe_map(). The operation depends on the backend and may fail
     * with ENOSYS. Use toHwFrame() with a transfer for a portable path.
     * @param frames
     * @param flags
     * @param ec
     * @return a frame mapped into the given pool
     */
    VideoFrame mapTo(const hw::FramesContext& frames,
                     int flags = AV_HWFRAME_MAP_READ,
                     OptionalErrorCode ec = throws()) const;

    /**
     * @brief Frames context of this hardware frame, or an invalid context.
     * @return the pool of this frame, or an invalid value
     */
    hw::FramesContext hwFramesContext() const noexcept;

    /**
     * @brief Device context used by this hardware frame, or an invalid context.
     * @return the device of this frame, or an invalid value
     */
    hw::DeviceContext hwDeviceContext() const noexcept;


    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired.
     *
     * @param data          data pointer to wrap
     * @param size          whole data buffer size with alignment and real data
     * @param pixelFormat   pixel format of the data
     * @param width         image width
     * @param height        image height
     * @param align         image data alignemnt
     *
     * @return image data wrapped into VideoFrame object
     */
    static VideoFrame wrap(const void *data, size_t size, PixelFormat pixelFormat, int width, int height, int align = 1);

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired.

     * @param data           external image data wrapped into std::span
     * @param pixelFormat    pixel format of the data
     * @param width          image width
     * @param height         image height
     * @param align          image data alignment
     * @return
     */
    static VideoFrame wrap(std::span<const std::byte> data, PixelFormat pixelFormat, int width, int height, int align = 1);

    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired. When data wants to be deleted, passed deleter function will be called.

     * @param data           external image data wrapped into std::span
     * @param deleter        user defined deleter
     * @param opaque         opaque data passed to the deleter, maybe nullptr
     * @param pixelFormat    pixel format of the data
     * @param width          image width
     * @param height         image height
     * @param align          image data alignment
     * @return
     */
    static VideoFrame wrap(std::span<const std::byte> data, void (*deleter)(void *opaque, uint8_t *data),
                           void *opaque, PixelFormat pixelFormat, int width, int height, int align = 1);
#endif
};

static_assert(std::is_copy_assignable_v<VideoFrame> == true);
static_assert(std::is_copy_constructible_v<VideoFrame> == true);
static_assert(std::is_move_assignable_v<VideoFrame> == true);
static_assert(std::is_move_constructible_v<VideoFrame> == true);

/**
 * One buffer of decoded audio samples.
 *
 * The samples follow the layout of their format: planar formats keep one plane
 * per channel, packed formats interleave the channels in the first plane.
 * isPlanar() reports which one this buffer uses.
 */
class AudioSamples : public Frame<AudioSamples>
{
public:
    using Frame<AudioSamples>::Frame;

    /**
     * Create an invalid buffer.
     */
    AudioSamples() = default;
    /**
     * Allocate an empty buffer with the given layout.
     *
     * @param sampleFormat  sample format of the buffer
     * @param samplesCount  number of samples per channel
     * @param channelLayout `AV_CH_LAYOUT_*` mask
     * @param sampleRate    sample rate in hertz
     * @param align         alignment in bytes for each plane
     */
    AudioSamples(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);
    /**
     * Copy external sample data into a buffer.
     *
     * @param data          buffer holding the samples
     * @param size          size of the buffer in bytes
     * @param sampleFormat  sample format of the data
     * @param samplesCount  number of samples per channel
     * @param channelLayout `AV_CH_LAYOUT_*` mask
     * @param sampleRate    sample rate in hertz
     * @param align         alignment in bytes for each plane
     */
    AudioSamples(const uint8_t *data, size_t size,
                  SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);

    /**
     * Copy constructor: references the same sample buffer.
     *
     * @param other the buffer to copy
     */
    AudioSamples(const AudioSamples &other);
    /**
     * Move constructor.
     *
     * @param other the buffer to move from; left invalid
     */
    AudioSamples(AudioSamples &&other);

    /**
     * Copy assignment: references the buffer of the other samples.
     *
     * @param rhs the buffer to assign from
     *
     * @return this buffer
     */
    AudioSamples& operator=(const AudioSamples &rhs);
    /**
     * Move assignment.
     *
     * @param rhs the buffer to move from; left invalid
     *
     * @return this buffer
     */
    AudioSamples& operator=(AudioSamples &&rhs);

    /**
     * Allocate the buffer storage.
     *
     * @param sampleFormat  sample format of the buffer
     * @param samplesCount  number of samples per channel
     * @param channelLayout `AV_CH_LAYOUT_*` mask
     * @param sampleRate    sample rate in hertz
     * @param align         alignment in bytes for each plane
     *
     * @return zero on success, a negative FFmpeg error code otherwise
     */
    int            init(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);

    /**
     * Sample format of the buffer.
     *
     * @return the format, or `AV_SAMPLE_FMT_NONE` for an invalid buffer
     */
    SampleFormat   sampleFormat() const;
    /**
     * Number of samples per channel.
     *
     * @return the sample count, or zero for an invalid buffer
     */
    int            samplesCount() const;
    /**
     * Number of audio channels.
     *
     * @return the channel count, or zero for an invalid buffer
     */
    int            channelsCount() const;
    /**
     * Channel layout as a bit mask.
     *
     * @return the `AV_CH_LAYOUT_*` mask, or zero when it is not set
     */
    uint64_t       channelsLayout() const;
    /**
     * Sample rate of the buffer.
     *
     * @return samples per second, or zero for an invalid buffer
     */
    int            sampleRate() const;
    /**
     * Number of bits per sample.
     *
     * @param ec holder for the error code
     *
     * @return the bit depth of the sample format
     */
    size_t         sampleBitDepth(OptionalErrorCode ec = throws()) const;
    /**
     * Whether the channels are stored in separate planes.
     *
     * @return `true` for planar formats, `false` for packed ones
     */
    bool           isPlanar() const;

    /**
     * Human-readable name of the channel layout.
     *
     * @return a string such as "stereo", or an empty string when the layout is
     *         unknown
     */
    std::string    channelsLayoutString() const;
};

static_assert(std::is_copy_assignable_v<AudioSamples> == true);
static_assert(std::is_copy_constructible_v<AudioSamples> == true);
static_assert(std::is_move_assignable_v<AudioSamples> == true);
static_assert(std::is_move_constructible_v<AudioSamples> == true);


} // ::av

