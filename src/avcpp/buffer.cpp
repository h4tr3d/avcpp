#include "avcompat.h"

#include "buffer.h"
#include "avcpp/avutils.h"

#include <cassert>
#include <utility>

namespace av {

namespace buffer {
/**
 * Buffer deleter that do nothing. To wrap static data.
 */
void null_deleter(void* /*opaque*/, uint8_t* /*data*/) {}

static AVBufferRef* ref(const AVBufferRef* buf) {
#if AVCPP_AVUTIL_VERSION_INT >= AV_VERSION_INT(57, 5, 101)
    return av_buffer_ref(buf);
#else
    return av_buffer_ref(const_cast<AVBufferRef*>(buf));
#endif
}
} // ::buffer


BufferRefView::BufferRefView(BufferRef &ref)
    : BufferRefView(ref.m_raw)
{}

BufferRefView::BufferRefView(const BufferRef &ref)
    : BufferRefView(ref.m_raw)
{}

AVBufferRef *BufferRefView::makeRef(iam_sure_what_i_do_tag) const noexcept {
    return m_raw ? av::buffer::ref(m_raw) : nullptr;
}

BufferRef BufferRefView::ref()
{
    if (!m_raw) [[unlikely]]
        return {};
    return BufferRef::ref(m_raw);
}

BufferRef BufferRefView::clone(int flags) const noexcept
{
    if (!m_raw)
        return {};
    return BufferRef(m_raw->data, m_raw->size, flags);
}

bool BufferRefView::isWritable() const noexcept
{
    return m_raw ? av_buffer_is_writable(m_raw) : false;
}

int BufferRefView::refCount() const noexcept
{
    return m_raw ? av_buffer_get_ref_count(m_raw) : 0;
}

std::size_t BufferRefView::size() const noexcept
{
    return m_raw ? m_raw->size : 0;
}

const uint8_t *BufferRefView::data() const noexcept
{
    return m_raw ? m_raw->data : nullptr;
}

const uint8_t *BufferRefView::constData() const noexcept
{
    return data();
}

uint8_t *BufferRefView::data(OptionalErrorCode ec)
{
    if (!m_raw)
        return nullptr;

    clear_if(ec);
    if (!isWritable()) {
        throws_if(ec, Errors::BufferReadonly);
        return nullptr;
    }
    return m_raw->data;
}

#if AVCPP_CXX_STANDARD >= 20
std::span<const uint8_t> BufferRefView::span() const noexcept
{
    if (!m_raw)
        return {};
    return {m_raw->data, std::size_t(m_raw->size)};
}

std::span<const uint8_t> BufferRefView::constSpan() const noexcept
{
    return span();
}

std::span<uint8_t> BufferRefView::span(OptionalErrorCode ec)
{
    if (!m_raw)
        return {};
    clear_if(ec);
    if (!isWritable()) {
        throws_if(ec, Errors::BufferReadonly);
        return {};
    }
    return {m_raw->data, std::size_t(m_raw->size)};
}
#endif // AVCPP_CXX_STANDARD >= 20

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


BufferRef::BufferRef(std::size_t size, bool keepUninit) noexcept
{
    if (keepUninit) [[likely]]
        m_raw = av_buffer_alloc(size);
    else
        m_raw = av_buffer_allocz(size);
}

BufferRef::BufferRef(uint8_t *data, size_t size, void (*free)(void *, uint8_t *), void *opaque, int flags) noexcept
{
    m_raw = av_buffer_create(data, size, free, opaque, flags);
}

BufferRef::BufferRef(const uint8_t *data, size_t size, void (*free)(void *, uint8_t *), void *opaque, int flags) noexcept
{
    m_raw = av_buffer_create(const_cast<uint8_t*>(data), size, free, opaque, flags | AV_BUFFER_FLAG_READONLY);
}

BufferRef::BufferRef(const uint8_t *data, size_t size, int flags) noexcept
{
    auto mem = av::memdup<uint8_t>(data, size);
    if (!mem)
        return;
    m_raw = av_buffer_create(mem.get(), size, av_buffer_default_free, nullptr, flags);
    if (m_raw)
        mem.release(); // memory owned by the buffer now
}

#if AVCPP_CXX_STANDARD
BufferRef::BufferRef(std::span<uint8_t> data, void (*free)(void *, uint8_t *), void *opaque, int flags) noexcept
    : BufferRef(data.data(), data.size(), free, opaque, flags)
{
}

BufferRef::BufferRef(std::span<const uint8_t> data, int flags) noexcept
    : BufferRef(data.data(), data.size(), flags)
{
}
#endif // AVCPP_CXX_STANDARD

BufferRef::BufferRef(const BufferRef &other) noexcept
    : BufferRef(av::buffer::ref(other.m_raw))
{
}

BufferRef::BufferRef(BufferRef &&other) noexcept
{
    swap(other);
}

BufferRef::~BufferRef() noexcept
{
    av_buffer_unref(&m_raw);
}

BufferRef &av::BufferRef::operator=(const BufferRef &other) noexcept
{
    if (this != std::addressof(other))
        BufferRef(other).swap(*this);
    return *this;
}

BufferRef BufferRef::wrap(const AVBufferRef *buf, int flags) noexcept
{
    return BufferRef{buf->data, std::size_t(buf->size), flags};
}

BufferRef BufferRef::wrap(AVBufferRef *buf) noexcept
{
    return BufferRef{buf};
}

BufferRef BufferRef::ref(const AVBufferRef *buf) noexcept
{
    return BufferRef{av::buffer::ref(buf)};
}

AVBufferRef *BufferRef::release() noexcept
{
    return std::exchange(m_raw, nullptr);
}

void BufferRef::reset() noexcept
{
    if (!m_raw)
        return;
    av_buffer_unref(&m_raw);
    assert(m_raw == nullptr);
}


void BufferRef::makeWritable(OptionalErrorCode ec) {
    clear_if(ec);
    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (auto const ret = av_buffer_make_writable(&m_raw); ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
        return;
    }
}

void BufferRef::resize(std::size_t size, OptionalErrorCode ec) {
    clear_if(ec);
    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (auto const ret = av_buffer_realloc(&m_raw, size); ret < 0) {
        throws_if(ec, ret, ffmpeg_category());
        return;
    }
}

void BufferRef::swap(BufferRef &other) noexcept
{
    using std::swap;
    swap(m_raw, other.m_raw);
}

BufferRef &av::BufferRef::operator=(BufferRef &&other) noexcept
{
    if (this != std::addressof(other))
        BufferRef(std::move(other)).swap(*this);
    return *this;
}

} // ::av