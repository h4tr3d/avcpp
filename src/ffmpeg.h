#pragma once

#include <iostream>

#include "avcompat.h"

extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/parseutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavformat/version.h>
#include <libavcodec/version.h>
#include <libavutil/bswap.h>
}


template<typename T>
struct FFWrapperPtr
{
    FFWrapperPtr() = default;
    explicit FFWrapperPtr(T *raw)
        : m_raw(raw) {}

    const T* raw() const             { return m_raw; }
    T*       raw()                   { return m_raw; }
    void     reset(T *raw = nullptr) { m_raw = raw; }
    bool     isNull() const          { return (m_raw == nullptr); }

    void _log(int level, const char *fmt) const
    {
        av_log(m_raw, level, fmt);
    }

    template<typename... Args>
    void _log(int level, const char* fmt, const Args&... args) const
    {
        av_log(m_raw, level, fmt, args...);
    }

protected:
    T *m_raw = nullptr;
};

#define RAW_GET(field, def) (m_raw ? m_raw->field : (def))
#define RAW_SET(field, val) if(m_raw) m_raw->field = (val)

#define RAW_GET2(cond, field, def) (m_raw && (cond) ? m_raw->field : def)
#define RAW_SET2(cond, field, val) if(m_raw && (cond)) m_raw->field = (val)

#define IF_GET(ptr, field, def) ((ptr) ? ptr->field : def)
#define IF_SET(ptr, field, val) (if(ptr) ptr->field = (val))

#define IF_GET2(cond, ptr, field, def) (ptr && (cond) ? ptr->field : def)
#define IF_SET2(cond, ptr, field, val) (if(ptr && (cond)) ptr->field = (val))

#if !DEPRECATED_INIT_PACKET
template<typename T>
struct FFWrapperRef
{
    FFWrapperRef() = default;
    explicit FFWrapperRef(const T &raw)
        : m_raw(raw) {}

    const T* raw() const               { return &m_raw; }
    T*       raw()                     { return &m_raw; }
    void     reset(const T &raw = T()) { m_raw = raw; }
    bool     isNull() const            {
        static const T empty = T();
        auto res = memcmp(&m_raw, &empty, sizeof(empty));
        return (res != 0);
    }

    void _log(int level, const char *fmt) const
    {
        av_log(&m_raw, level, fmt);
    }

    template<typename... Args>
    void _log(int level, const char* fmt, const Args&... args) const
    {
        av_log(&m_raw, level, fmt, args...);
    }

protected:
    T m_raw = T();
};
#endif

template<typename WrapperClass, typename T, T NoneValue = static_cast<T>(-1)>
struct PixSampleFmtWrapper
{
    constexpr PixSampleFmtWrapper() = default;
    constexpr PixSampleFmtWrapper(T fmt) noexcept : m_fmt(fmt) {}

    // Access to  the stored value
    operator T() const noexcept
    {
        return m_fmt;
    }

    T get() const noexcept
    {
        return m_fmt;
    }

    operator T&() noexcept
    {
        return m_fmt;
    }

    WrapperClass& operator=(T fmt) noexcept
    {
        m_fmt = fmt;
        return *this;
    }

    void set(T fmt) noexcept
    {
        m_fmt = fmt;
    }

    // IO Stream interface
    friend std::ostream& operator<<(std::ostream& ost, WrapperClass fmt)
    {
        ost << fmt.name();
        return ost;
    }

protected:
    T m_fmt = NoneValue;
};
