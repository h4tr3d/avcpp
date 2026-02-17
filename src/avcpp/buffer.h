#pragma once

#include "avcpp/averror.h"
#include "ffmpeg.h"

#if AVCPP_CXX_STANDARD >= 20
#include <span>
#endif // AVCPP_CXX_STANDARD >= 20

namespace av {

namespace buffer {
/**
 * Buffer deleter that do nothing. To wrap static data.
 */
void null_deleter(void* /*opaque*/, uint8_t* /*data*/);
} // ::buffer

class BufferRef;

/**
 * Non-owning view for the nested AVBufferRef
 */
class BufferRefView : public FFWrapperPtr<AVBufferRef>
{
public:
    using FFWrapperPtr<AVBufferRef>::FFWrapperPtr;

    /**
     * Construct null buffer
     */
    BufferRefView() = default;

    explicit BufferRefView(BufferRef& ref);
    explicit BufferRefView(const BufferRef& ref);

    /**
     * Tag to confirm usage of the Low Level functionality
     */
    struct iam_sure_what_i_do_tag{};
    /**
     * Make an reference of the exsting buffer. Useful to pass to the low-level API.
     * @return new reference on success (data kept uncopied) or nullptr if control block allocation fails.
     */
    AVBufferRef* makeRef(iam_sure_what_i_do_tag) const noexcept;

    /**
     * Create reference to the view data, refCount() will be increased
     * @return
     */
    BufferRef ref();

    /**
     * Make deep copy of the existing buffer. Data will be copied into new buffer and owning taken.
     * @param flags AV_BUFFER_FLAG_*
     * @return
     */
    BufferRef clone(int flags = 0) const noexcept;

    /**
     * Report current reference counter of the buffer. For the refCount() > 1 isWritable() always return false.
     * @return
     */
    int refCount() const noexcept;
    /**
     * Report writable flag. Always false if refCount() == 0 or > 1. Otherwise controls by the AV_BUFFER_FLAG_READONLY.
     * @return
     */
    bool isWritable() const noexcept;

    /**
     * Nested buffer size
     * @return
     */
    std::size_t size() const noexcept;
    /**
     * Pointer to the data block start
     * @return
     */
    const uint8_t* data() const noexcept;
    /**
     * Force request const data
     * @return
     */
    const uint8_t* constData() const noexcept;
    /**
     * Pointer to the data block start. Note, write access possible only to the buffer when isWritable()==true.
     * If buffer can't be written error code will reported or exception reised.
     * @param ec
     * @return
     */
    uint8_t* data(OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Span interface to obtain nested buffer data.
     * @return
     */
    std::span<const uint8_t> span() const noexcept;
    /**
     * Force request const span
     * @return
     */
    std::span<const uint8_t> constSpan() const noexcept;
    /**
     * Span interface to obtain nested buffer data. Like data(ec) possible only for writable buffers.
     * @param ec
     * @return
     */
    std::span<uint8_t> span(OptionalErrorCode ec = throws());
#endif
};


/**
 * Light weight wrapper for the FFmpeg AVBufferRef functionality
 */
class BufferRef : public BufferRefView
{
    //using FFWrapperPtr<AVBufferRef>::FFWrapperPtr;
    using BufferRefView::BufferRefView;

public:
    /**
     * Construct null buffer
     */
    BufferRef() = default;

    /**
     * Allocate referenced buffer with given size. If @a keepUninit point into true data in the buffer kept uninitialized.
     * otherwise it initializes with zeroes.
     *
     * refCount() will return 1 after this function.
     *
     * @param size        requested size of the buffer
     * @param keepUninit  true to kept buffer with garbage and zeroing it otherwise.
     */
    BufferRef(std::size_t size, bool keepUninit = true) noexcept;

    /**
     * Wrap existing data buffer and take ownershipping. Data kept uncopied.
     *
     * If data allocated with custom allocators other then av_malloc/av_realloc/av::alloc/av::memdup families, @p free
     * function must be provided.
     *
     * Also, it can be used to wrap static buffers with av::buffer::null_deleter()
     *
     * refCount() will return 1 after this function.
     *
     * @param data    data buffer to wrap, owning taken.
     * @param size    size of the data buffer.
     * @param free    deleter for the data, maybe nullptr if data buffer was allocated with
     *                av_malloc/av_realloc/av::alloc/av::memdup function familiy.
     * @param opaque  optional argnument for the deleter, may be null
     * @param flags   AV_BUFFER_FLAG_* flags
     */
    BufferRef(uint8_t *data, size_t size, void (*free)(void *opaque, uint8_t *data), void *opaque, int flags = 0) noexcept;

    /**
     * Same previous one, but forces AV_BUFFER_FLAG_READONLY. Data still uncopied until makeWritable() call.
     *
     * Can be used to wrap static constant data with av::buffer::null_deleter.
     *
     * @param data
     * @param size
     * @param opaque
     * @param flags
     */
    BufferRef(const uint8_t *data, size_t size, void (*free)(void *opaque, uint8_t *data), void *opaque, int flags = 0) noexcept;

    /**
     * Clone exsiting data buffer and wrap it. Data copied into new storage and owning taken.
     *
     * refCount() will return 1 after this function.
     *
     * @param data  data buffer with payload
     * @param size  data buffer size
     * @param flags AV_BUFFER_FLAG_*
     */
    BufferRef(const uint8_t *data, size_t size, int flags = 0) noexcept;

#if AVCPP_CXX_STANDARD
    /**
     * Same to BufferRef(data, size, free, opaque, flags) but pass data via std::span() interface. Span must be pointed
     * into start of the allocated block.
     *
     * @param data     data block reference with size
     * @param free     deleter, may be nullptr
     * @param opaque   extra data passed into deleter, maybe nullptr
     * @param flags    AV_BUFFER_FLAG_*
     */
    BufferRef(std::span<uint8_t> data, void (*free)(void *opaque, uint8_t *data), void *opaque, int flags = 0) noexcept;

    /**
     * Same to BufferRef(data, size, flags) but pass data via std::span() interface. Span may point to any location of
     * the allocated memory block.
     * @param data   data block reference with size
     * @param flags  AV_BUFFER_FLAG_*
     */
    explicit BufferRef(std::span<const uint8_t> data, int flags = 0) noexcept;
#endif // AVCPP_CXX_STANDARD

    /**
     * Make a buffer reference. refCount() will return values increased by 1 for both new and old instances. Actual
     * data will be kept uncopied. Newly created buffer may be invalid if the allocating for the reference block fails.
     * @param other
     */
    BufferRef(const BufferRef& other) noexcept;
    /**
     * Just move owning of the nested AVBufferRef pointer into new instance. refCount() kept untouched. @p other buffer
     * does not controll nested AVBufferRef anymore;
     * @param other
     */
    BufferRef(BufferRef&& other) noexcept;

    /**
     * Unref nested AVBufferRef. If the refCount() == 1 data will be deleted too.
     */
    ~BufferRef() noexcept;

    /**
     * Make a buffer reference. Existing data will be unreferenced. refCount() will return value increased by 1 for both
     * this and other instances. Value must be same.
     *
     * Buffer state may be invalid, if control block allocation will be fail durting copying.
     *
     * @param other
     * @return
     */
    BufferRef& operator=(const BufferRef& other) noexcept;
    /**
     * Move owning of the nested AVBufferRef from other to the this instance. Existing data will be unreferenced.
     * @param other
     * @return
     */
    BufferRef& operator=(BufferRef&& other) noexcept;

    /**
     * Wrap constant raw buffer reference. Data will be cloned into new buffer.
     * @param buf    buffer to wrap
     * @param flags  AV_BUFFER_FLAG_*
     * @return wrapped buffer reference. It may be invalid if allocation fails.
     */
    static BufferRef wrap(const AVBufferRef *buf, int flags = 0) noexcept;
    /**
     * Wrap non-constant raw buffer reference. Owning taken, but original buffer will not be referenced.
     *
     * Possible usage:
     * @code
     * BufferRef buf = BufferRef::wrap(av_buffer_ref(raw_buffer))
     * @endcode
     *
     * @param buf buffer to wrap
     * @return wrapped buffer reference. It may be invalid if allocation fails.
     */
    static BufferRef wrap(AVBufferRef *buf) noexcept;
    /**
     * Reference non-const buffer reference and wrap it. Owning of the new ference will be taken. Data kept uncopying.
     * @param buf raw buffer to reference
     * @return wrapped buffer reference. It may be invalid if allocation fails.
     */
    static BufferRef ref(const AVBufferRef *buf) noexcept;

    /**
     * Release owning of the buffer and return existing raw AVBufferRef. Useful to move owning without referencing
     * into nested low-level code.
     * @return
     */
    AVBufferRef* release() noexcept;

    /**
     * Force to unref nested data
     */
    void reset() noexcept;

    /**
     * Make buffer writable. Look into notice of the av_buffer_make_writable() for data coping details.
     * @param ec
     */
    void makeWritable(OptionalErrorCode ec = throws());
    /**
     * Resize given buffer. Look into of the av_buffer_realloc() for allocation details.
     * @param size   new buffer size
     * @param ec
     */
    void resize(std::size_t size, OptionalErrorCode ec = throws());

    /**
     * Swap nested data with other
     * @param other
     */
    void swap(BufferRef &other) noexcept;
};

} // ::av