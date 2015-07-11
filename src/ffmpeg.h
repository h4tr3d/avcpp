#ifndef FFMPEG_H
#define FFMPEG_H

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
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#if LIBAVFILTER_VERSION_INT <= AV_VERSION_INT(2,77,100) // 0.11.1
#  include <libavfilter/vsrc_buffer.h>
#endif
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
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


// Extended attributes
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_deprecated2(x) __attribute__((deprecated(x)))
#elif defined(_MSC_VER)
#    define attribute_deprecated2(x) __declspec(deprecated(x))
#else
#    define attribute_deprecated2(x)
#endif


#endif // FFMPEG_H
