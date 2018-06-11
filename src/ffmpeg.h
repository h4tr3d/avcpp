#ifndef FFMPEG_H
#define FFMPEG_H

#include <iostream>

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
}

extern "C" {
#include <libavfilter/avfilter.h>
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7,0,0)
#  include <libavfilter/avfiltergraph.h>
#endif
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#if LIBAVFILTER_VERSION_INT <= AV_VERSION_INT(2,77,100) // 0.11.1
#  include <libavfilter/vsrc_buffer.h>
#endif
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(6,31,100) // 3.0
#include <libavfilter/avcodec.h>
#endif
}

// Compat level

// avcodec
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
inline void avcodec_free_frame(AVFrame **frame)
{
    av_freep(frame);
}
#endif

// avfilter
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
inline const char *avfilter_pad_get_name(AVFilterPad *pads, int pad_idx)
{
    return pads[pad_idx].name;
}

inline AVMediaType avfilter_pad_get_type(AVFilterPad *pads, int pad_idx)
{
    return pads[pad_idx].type;
}
#endif


// Wrapper around av_free_packet()/av_packet_unref()
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(6,31,100) // < 3.0
#define avpacket_unref(p) av_free_packet(p)
#else
#define avpacket_unref(p) av_packet_unref(p)
#endif


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


template<typename WrapperClass, typename T, T NoneValue = static_cast<T>(-1)>
struct PixSampleFmtWrapper
{
    PixSampleFmtWrapper() = default;
    PixSampleFmtWrapper(T fmt) noexcept : m_fmt(fmt) {}

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

// Extended attributes
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_deprecated2(x) __attribute__((deprecated(x)))
#elif defined(_MSC_VER)
#    define attribute_deprecated2(x) __declspec(deprecated(x))
#else
#    define attribute_deprecated2(x)
#endif


#endif // FFMPEG_H
