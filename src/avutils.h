#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef __cpp_lib_print
#  include <print>
#endif

#include "ffmpeg.h"
#include "avtime.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavcodec/avcodec.h>
}

// WA: codecpar usage need more investigation. Temporary disable it.
#define USE_CODECPAR ((LIBAVCODEC_VERSION_MAJOR) >= 59) // FFmpeg 5.0

// New Audio Channel Layout API
#define API_NEW_CHANNEL_LAYOUT ((LIBAVUTIL_VERSION_MAJOR > 57) || (LIBAVUTIL_VERSION_MAJOR == 57 && (LIBAVUTIL_VERSION_MINOR >= 24)))
// `int64_t frame_num` has been added in the 60.2, in the 61.0 it should be removed
#define API_FRAME_NUM          ((LIBAVCODEC_VERSION_MAJOR > 60) || (LIBAVCODEC_VERSION_MAJOR == 60 && LIBAVCODEC_VERSION_MINOR >= 2))
// use AVFormatContext::url
#define API_AVFORMAT_URL       ((LIBAVFORMAT_VERSION_MAJOR > 58) || (LIBAVFORMAT_VERSION_MAJOR == 58 && LIBAVFORMAT_VERSION_MINOR >= 7))
// net key frame flags: AV_FRAME_FLAG_KEY (FFmpeg 6.1)
#define API_FRAME_KEY          ((LIBAVUTIL_VERSION_MAJOR > 58) || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR >= 29))
// avcodec_close() support
#define API_AVCODEC_CLOSE      (LIBAVCODEC_VERSION_MAJOR < 61)

#if defined(__ICL) || defined (__INTEL_COMPILER)
#    define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:1478))
#    define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#elif defined(_MSC_VER)
#    define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:4996))
#    define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#elif defined(__GNUC__) || defined(__clang__)
#    define FF_DISABLE_DEPRECATION_WARNINGS _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#    define FF_ENABLE_DEPRECATION_WARNINGS  _Pragma("GCC diagnostic warning \"-Wdeprecated-declarations\"")
#else
#    define FF_DISABLE_DEPRECATION_WARNINGS
#    define FF_ENABLE_DEPRECATION_WARNINGS
#endif

// sizeof(AVPacket) is no part of the public ABI, packet must be allocated in heap
#define API_AVCODEC_NEW_INIT_PACKET (LIBAVCODEC_VERSION_MAJOR >= 58)
// some fields in the AVCodec structure deprecard and replaced by the call of avcodec_get_supported_config()
#define API_AVCODEC_GET_SUPPORTED_CONFIG (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100))
// av_stream_get_codec_timebase() deprecard now without replacement
#define API_AVFORMAT_AV_STREAM_GET_CODEC_TIMEBASE (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(61, 5, 101))

// Allow to use spece-ship operator, whem possible
#if __has_include(<compare>)
#  include <compare>
#  if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907
#    define AVCPP_USE_SPACESHIP_OPERATOR 1
#  endif
#endif

//
// Functions
//
namespace av {

// Basic FFmpeg constants
constexpr auto NoPts = static_cast<int64_t>(AV_NOPTS_VALUE);
constexpr auto TimeBase = static_cast<int>(AV_TIME_BASE);
constexpr auto TimeBaseQ = AVRational{1, AV_TIME_BASE};


template<typename R, typename T>
R lexical_cast(const T& v)
{
    R result;
    std::stringstream ss;
    ss << v;
    ss >> result;
    return result;
}

class noncopyable
{
protected:
    noncopyable() = default;
    noncopyable( const noncopyable& ) = delete;
    void operator=( const noncopyable& ) = delete;
};

/**
 * This method can be used to turn up or down FFmpeg's logging level.
 *
 * @param level  An integer value for level.  Lower numbers
 *               mean less logging.  A negative number tells FFmpeg to
 *               shut up.
 */
void set_logging_level(int32_t level);


/**
 * Like @see set_logging_level, but can assept logging level as string.
 *
 * @param level - string representation of loggin level:
 *                'quiet', 'panic', 'fatal', 'error', 'warning', 'info', 'verbose' or 'debug',
 *                it also can be numeric (but in string representation), it it case
 *                boost::lexical_cast will be used and set_logging_level(int32_t) will be
 *                called.
 */
void set_logging_level(const std::string& level);

// Compat code
#define setFFmpegLoggingLevel set_logging_level

/**
 * @brief dump_binary_buffer
 * Dump binary buffer to std out in HEX view
 * @param buffer        pointer to buffer start
 * @param buffer_size   buffer size
 * @param width         output width in hex values (real text screen with is: sw = width * 3 - 1)
 */
void dumpBinaryBuffer(uint8_t *buffer, int buffer_size, int width = 16);


/**
 * C++ verstion of the av_err2str()
 * @param error - error code to convert to string
 * @return string representation of error code
 */
std::string error2string(int error);

}




//
// Classes
//
namespace av {

struct EmptyDeleter
{
    void operator()(void *) {}
};

/**
 * @brief The AvDeleter struct
 * Unified delete functor for variois FFMPEG/libavformat/libavcodec and so on resource allocators.
 * Can be used with shared_ptr<> and so on.
 */
namespace v1 {
struct AvDeleter
{
    bool operator() (struct SwsContext* &swsContext);
    bool operator() (struct AVCodecContext* &codecContext);
    bool operator() (struct AVOutputFormat* &format);
    bool operator() (struct AVFormatContext* &formatContext);
    bool operator() (struct AVFrame* &frame);
    bool operator() (struct AVPacket* &packet);
    bool operator() (struct AVDictionary* &dictionary);
    bool operator ()(struct AVFilterInOut* &filterInOut);
};
} // ::v1

inline namespace v2 {
struct SmartDeleter
{
    template<typename T>
    bool operator() (T *ptr) {
        return v1::AvDeleter()(ptr);
    }
};
}

template<typename T>
std::unique_ptr<T, void(*)(void*)> malloc(size_t size)
{
    return {static_cast<T*>(av_malloc(size)), av_free};
}

template<typename T>
std::unique_ptr<T, void(*)(void*)> mallocz(size_t size)
{
    return {static_cast<T*>(av_mallocz(size)), av_free};
}

template<typename T>
std::unique_ptr<T, void(*)(void*)> memdup(const void *p, size_t size)
{
    return {static_cast<T*>(av_memdup(p, size)), av_free};
}

/**
 * Functor to take next element in list/array
 */
#if 0
struct AvNextElement
{
    AVFilterInOut * operator()(AVFilterInOut * x) const
    {
        if (x)
            return x->next;
        else
            return 0;
    }
};
#endif



/**
 * Teamplate class to set value for variable when out of scope will be occured. Use RAII idiom.
 *
 * By default type of variable and value is same, but it can be simple overriden, use
 *  @code
 *    ScopedValue<VariableType, OutValueType> scopedValue(variable, false);
 *  @endcode
 * instead
 * @code
 *    ScopedValue<VariableType> scopedValue(variable, false);
 * @endcode
 */
template<typename T, typename V = T>
class ScopedValue
{
public:
    /**
     * Ctor. Store reference to variable and output value.
     * @param var      variable that must be set
     * @param outValue value of variable
     */
    ScopedValue(T &var, const V& outValue)
        : var(var),
          outValue(outValue)
    {
    }

    /**
     * Ctor. Like previous one but automaticaly can assign initial value to variable.
     * @param var      variable that must be set
     * @param inValue  initial value
     * @param outValue output value
     */
    ScopedValue(T &var, const V &inValue, const V &outValue)
        : var(var),
          outValue(outValue)
    {
        this->var = inValue;
    }

    ~ScopedValue()
    {
        var = outValue;
    }

private:
    T& var;
    V  outValue;
};


/**
 * @brief The ScopeOutAction class - guard-type class that allows points callback that will be called
 * at the scope out
 *
 * Example:
 * @code
 * void foo()
 * {
 *     int fd = open(...some args...);
 *     ScopeOutAction action([fd](){
 *         close(fd);
 *     });
 *
 *     try
 *     {
 *         // some actions that can throw exception
 *     }
 *     catch(...)
 *     {
 *         throw; // Not best-practice, only for example
 *     }
 * }
 * @endcode
 *
 * In example above dtor of the action will be called before fd and we correctly close descriptor.
 *
 */
class ScopeOutAction
{
public:
    template<typename Proc>
    ScopeOutAction(const Proc& proc)
        : m_proc(proc)
    {}

    template<typename Proc>
    ScopeOutAction(Proc&& proc)
        : m_proc(std::forward<Proc>(proc))
    {}

    ~ScopeOutAction()
    {
        if (m_proc)
            m_proc();
    }

private:
    std::function<void()> m_proc;
};


////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct EqualComparator
{
    EqualComparator(const T& value)
        : value(value)
    {}

    bool operator() (const T& value) const
    {
        if (this->value == value)
            return true;

        return false;
    }

    const T& value;
};

/**
 * Select more approptiate value from given value list. Useful for AVCodec::supported_framerates,
 * AVCodec::pix_fmts and so on.
 *
 * T - type of value and (by default) list elements
 * L - type of list elements
 * C - end list comparator
 *
 * If T and L different types, T must be have ctor from L.
 *
 * @param value           value to set
 * @param list            list of allowed values
 * @param endOfListValue  end of list value, like
 * @return value if list null or if it present in list, or more appropriate value from list
 */
template<typename T, typename L, typename C>
T guessValue(const T& value, const L * list, C endListComparator)
{
    if (!list)
        return value;

    // move values to array
    std::deque<T> values;
    for (const L * ptr = list; !endListComparator(*ptr); ++ptr)
    {
        T v = *ptr;
        values.push_back(v);
    }

    // sort list
    std::sort(values.begin(), values.end());

    // Search more appropriate range
    int begin = 0;
    int end   = values.size() - 1;
    while ((end - begin) > 1)
    {
        int mid = begin + (end - begin) / 2;

        if (value <= values[mid])
        {
            end = mid;
        }
        else
        {
            begin = mid;
        }
    }

    // distance from VALUE to BEGIN more short or VALUE less then BEGIN
    if (value <= values[begin] || (value - values[begin]) < (values[end] - value))
    {
        return values[begin];
    }

    return values[end];
}


template<typename T, typename Container>
void array_to_container(const T* array, std::size_t nelements, Container &container)
{
    if (!array || nelements == 0)
        return;
    std::copy_n(array, array + nelements, std::back_inserter(container));
}

template<typename T, typename Container, typename Callable>
void array_to_container(const T* array, std::size_t nelements, Container &container, Callable convert)
{
    if (!array || nelements == 0)
        return;
    // TBD: implement in more clean way
    //std::copy_n(array, array + nelemnts, std::back_inserter(container));
    for (auto i = 0u; i < nelements; ++i) {
        container.push_back(convert(array[i]));
    }
}

template<typename T, typename Container, typename Compare>
void array_to_container(const T* array, Container &container, Compare isEnd)
{
    if (!array)
        return;
    T value;
    while (!isEnd(value = *array++))
        container.push_back(value);
}

template<typename T, typename Container, typename Compare, typename Callable>
void array_to_container(const T* array, Container &container, Compare isEnd, Callable convert)
{
    if (!array)
        return;
    T value;
    while (!isEnd(value = *array++))
        container.push_back(convert(value));
}


// printing
#ifdef __cpp_lib_print
using std::print;
#else

namespace internal {
struct fp_output_iterator
{
    /// One of the @link iterator_tags tag types@endlink.
    typedef std::output_iterator_tag iterator_category;
    /// The type "pointed to" by the iterator.
    typedef void value_type;
    /// Distance between iterators is represented as this type.
    typedef ptrdiff_t difference_type;
    /// This type represents a pointer-to-value_type.
    typedef void pointer;
    /// This type represents a reference-to-value_type.
    typedef void reference;

private:
    std::FILE *m_fp{nullptr};

public:
    fp_output_iterator(std::FILE *out) noexcept
        : m_fp(out)
    {}

    fp_output_iterator(const fp_output_iterator& obj) noexcept
        : m_fp(obj.m_fp)
    {}
    fp_output_iterator& operator=(const fp_output_iterator&) = default;

    fp_output_iterator& operator=(char ch) {
        fputc(ch, m_fp);
        return *this;
    }

    fp_output_iterator& operator=(const char *str) {
        fputs(str, m_fp);
        return *this;
    }

    [[nodiscard]] fp_output_iterator& operator*() noexcept { return *this; }
    fp_output_iterator& operator++() noexcept { return *this; }
    fp_output_iterator& operator++(int) noexcept { return *this; }
};
} // ::internal

template<class... Args>
void print(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args)
{
    internal::fp_output_iterator out{stream};
    std::format_to(out, std::move(fmt), std::forward<Args>(args)...);
}

template<class... Args>
void print(std::format_string<Args...> fmt, Args&&... args)
{
    av::print(stdout, std::move(fmt), std::forward<Args>(args)...);
}

template<class... Args>
std::ostream& print(std::ostream &stream, std::format_string<Args...> fmt, Args&&... args)
{
    std::ostream_iterator<char> out{stream};
    std::format_to(out, std::move(fmt), std::forward<Args>(args)...);
    return stream;
}
#endif

} // ::av

