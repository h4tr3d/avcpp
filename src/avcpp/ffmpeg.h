#pragma once

/**
 * FFmpeg types re-exported for the public API and small value wrappers.
 *
 * @file
 * @ingroup core
 */

#include <iostream>

#include "avcompat.h"

extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/parseutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/bswap.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}


/**
 * Base for wrappers that own a pointer to an FFmpeg object.
 *
 * The template stores the pointer, provides checked access to it and the
 * logging helpers every wrapper exposes. It does not manage the lifetime of
 * the pointed object: derived classes call `av_*_free` in their destructors.
 *
 * @tparam T the FFmpeg type being wrapped
 */
template<typename T>
struct FFWrapperPtr
{
    /**
     * Create an empty wrapper; `isNull()` is true.
     */
    FFWrapperPtr() = default;
    /**
     * Wrap an existing object.
     *
     * @param raw pointer to take; the wrapper does not free it
     */
    explicit FFWrapperPtr(T *raw)
        : m_raw(raw) {}

    /**
     * Read access to the wrapped object.
     *
     * @return the stored pointer, or nullptr when the wrapper is empty
     */
    const T* raw() const             { return m_raw; }
    /**
     * Write access to the wrapped object.
     *
     * @return the stored pointer, or nullptr when the wrapper is empty
     */
    T*       raw()                   { return m_raw; }
    /**
     * Replace the stored pointer.
     *
     * @param raw the new pointer; nullptr empties the wrapper
     *
     * @note The previous object is not freed: lifetime belongs to the derived
     *       class.
     */
    void     reset(T *raw = nullptr) { m_raw = raw; }
    /**
     * Whether the wrapper is empty.
     *
     * @return `true` when no object is stored
     */
    bool     isNull() const          { return (m_raw == nullptr); }

    /**
     * Log through the FFmpeg log system, using this wrapper as the context.
     *
     * @param level an `AV_LOG_*` level
     * @param fmt   printf-style format string without arguments
     */
    void _log(int level, const char *fmt) const
    {
        av_log(m_raw, level, fmt);
    }

    /**
     * Log through the FFmpeg log system with arguments.
     *
     * @param level an `AV_LOG_*` level
     * @param fmt   printf-style format string
     * @param args  arguments for the format string
     */
    template<typename... Args>
    void _log(int level, const char* fmt, const Args&... args) const
    {
        av_log(m_raw, level, fmt, args...);
    }

protected:
    /// The wrapped pointer; owned by the derived class, not by this template.
    T *m_raw = nullptr;
};

/**
 * Read an AVCodecContext-style field through a possibly empty raw pointer.
 *
 * @param field the field to read
 * @param def   the value to return when the pointer is empty
 */
#define RAW_GET(field, def) (m_raw ? m_raw->field : (def))
/**
 * Write an AVCodecContext-style field when the raw pointer is set.
 *
 * @param field the field to write
 * @param val   the value to store
 */
#define RAW_SET(field, val) if(m_raw) m_raw->field = (val)

/**
 * Read a field when the pointer is set and a condition holds.
 *
 * @param cond  extra condition, usually `isValid()`
 * @param field the field to read
 * @param def   the value to return otherwise
 */
#define RAW_GET2(cond, field, def) (m_raw && (cond) ? m_raw->field : def)
/**
 * Write a field when the pointer is set and a condition holds.
 *
 * @param cond  extra condition, usually `!isOpened()`
 * @param field the field to write
 * @param val   the value to store
 */
#define RAW_SET2(cond, field, val) if(m_raw && (cond)) m_raw->field = (val)

/**
 * Read a field through an explicit pointer, or return a default.
 *
 * @param ptr   the pointer to test
 * @param field the field to read
 * @param def   the value to return when the pointer is null
 */
#define IF_GET(ptr, field, def) ((ptr) ? ptr->field : def)
/**
 * Write a field through an explicit pointer when it is not null.
 *
 * @param ptr   the pointer to test
 * @param field the field to write
 * @param val   the value to store
 */
#define IF_SET(ptr, field, val) (if(ptr) ptr->field = (val))

/**
 * Read a field through a pointer when it is not null and a condition holds.
 *
 * @param cond  extra condition
 * @param ptr   the pointer to test
 * @param field the field to read
 * @param def   the value to return otherwise
 */
#define IF_GET2(cond, ptr, field, def) (ptr && (cond) ? ptr->field : def)
/**
 * Write a field through a pointer when it is not null and a condition holds.
 *
 * @param cond  extra condition
 * @param ptr   the pointer to test
 * @param field the field to write
 * @param val   the value to store
 */
#define IF_SET2(cond, ptr, field, val) (if(ptr && (cond)) ptr->field = (val))

#if !DEPRECATED_INIT_PACKET
/**
 * Base for wrappers that hold a small FFmpeg structure by value.
 *
 * Used for structures FFmpeg expects to be passed by pointer but that have no
 * separate allocation, such as AVPacketSideData. Address stability is part of
 * the contract: `raw()` returns a pointer into the wrapper, so the wrapper must
 * outlive any FFmpeg call that uses it.
 *
 * @tparam T the FFmpeg type being held
 */
template<typename T>
struct FFWrapperRef
{
    /**
     * Create a value-initialised holder.
     */
    FFWrapperRef() = default;
    /**
     * Copy a structure into the wrapper.
     *
     * @param raw the value to store
     */
    explicit FFWrapperRef(const T &raw)
        : m_raw(raw) {}

    /**
     * Read access to the stored structure.
     *
     * @return a pointer to the value inside the wrapper
     */
    const T* raw() const               { return &m_raw; }
    /**
     * Write access to the stored structure.
     *
     * @return a pointer to the value inside the wrapper
     */
    T*       raw()                     { return &m_raw; }
    /**
     * Replace the stored value.
     *
     * @param raw the new value; the default resets to a zero-filled structure
     */
    void     reset(const T &raw = T()) { m_raw = raw; }
    /**
     * Whether the stored value equals a default-constructed one.
     *
     * @return `true` when every byte matches `T()`
     */
    bool     isNull() const            {
        static const T empty = T();
        auto res = memcmp(&m_raw, &empty, sizeof(empty));
        return (res == 0);
    }

    /**
     * Log through the FFmpeg log system, using this wrapper as the context.
     *
     * @param level an `AV_LOG_*` level
     * @param fmt   printf-style format string without arguments
     */
    void _log(int level, const char *fmt) const
    {
        av_log(&m_raw, level, fmt);
    }

    /**
     * Log through the FFmpeg log system with arguments.
     *
     * @param level an `AV_LOG_*` level
     * @param fmt   printf-style format string
     * @param args  arguments for the format string
     */
    template<typename... Args>
    void _log(int level, const char* fmt, const Args&... args) const
    {
        av_log(&m_raw, level, fmt, args...);
    }

protected:
    /// The stored value.
    T m_raw = T();
};
#endif

/**
 * Strongly typed enum-like wrapper over an FFmpeg pixel or sample format.
 *
 * The concrete wrappers (PixelFormat, SampleFormat) add name lookup and
 * comparison; this template supplies the storage and the conversions that let
 * a wrapper be passed wherever FFmpeg expects the raw enum.
 *
 * @tparam WrapperClass the derived type, for the assignment operator
 * @tparam T            the FFmpeg enum type
 * @tparam NoneValue     the value meaning "unset", usually -1
 */
template<typename WrapperClass, typename T, T NoneValue = static_cast<T>(-1)>
struct PixSampleFmtWrapper
{
    /**
     * Create a wrapper holding `NoneValue`.
     */
    constexpr PixSampleFmtWrapper() = default;
    /**
     * Wrap a raw enum value.
     *
     * @param fmt the value to store
     */
    constexpr PixSampleFmtWrapper(T fmt) noexcept : m_fmt(fmt) {}

    // Access to  the stored value
    /**
     * Implicit conversion to the FFmpeg enum.
     *
     * @return the stored value, so the wrapper can be passed to FFmpeg calls
     */
    operator T() const noexcept
    {
        return m_fmt;
    }

    /**
     * The stored enum value.
     *
     * @return the stored value
     */
    T get() const noexcept
    {
        return m_fmt;
    }

    /**
     * Mutable access to the stored value.
     *
     * Lets FFmpeg fill the value in place, for example through a pointer
     * parameter.
     *
     * @return a reference to the stored value
     */
    operator T&() noexcept
    {
        return m_fmt;
    }

    /**
     * Assign a raw enum value.
     *
     * @param fmt the value to store
     *
     * @return this wrapper
     */
    WrapperClass& operator=(T fmt) noexcept
    {
        m_fmt = fmt;
        return *this;
    }

    /**
     * Assign a raw enum value.
     *
     * @param fmt the value to store
     */
    void set(T fmt) noexcept
    {
        m_fmt = fmt;
    }

    // IO Stream interface
    /**
     * Print the format name.
     *
     * @param ost the stream to write to
     * @param fmt the wrapper to print
     *
     * @return the stream, for chaining
     */
    friend std::ostream& operator<<(std::ostream& ost, WrapperClass fmt)
    {
        ost << fmt.name();
        return ost;
    }

protected:
    /// The stored enum value.
    T m_fmt = NoneValue;
};
