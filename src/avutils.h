#ifndef AV_UTILS_H
#define AV_UTILS_H

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>

#include "ffmpeg.h"
#include "packet.h"
#include "frame.h"
#include "avtime.h"

extern "C" {
#include <libavfilter/avfilter.h>
}

#define USE_CODECPAR (LIBAVFORMAT_VERSION_MAJOR >= 58)

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

//
// Functions
//
namespace av {

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
struct AvDeleter
{
    bool operator() (SwsContext* &swsContext)
    {
        sws_freeContext(swsContext);
        swsContext = 0;
        return true;
    }


    bool operator() (AVCodecContext* &codecContext)
    {
        avcodec_close(codecContext);
        av_free(codecContext);
        codecContext = 0;
        return true;
    }


    bool operator() (AVOutputFormat* &format)
    {
        // Only set format to zero, it can'be freed by user
        format = 0;
        return true;
    }


    bool operator() (AVFormatContext* &formatContext)
    {
        avformat_free_context(formatContext);
        formatContext = 0;
        return true;
    }

    bool operator() (AVFrame* &frame)
    {
        av_freep(&frame);
        frame = 0;
        return true;
    }


    bool operator() (AVPacket* &packet)
    {
        avpacket_unref(packet);
        av_free(packet);
        packet = 0;
        return true;
    }


    bool operator() (AVDictionary* &dictionary)
    {
        av_dict_free(&dictionary);
        dictionary = 0;
        return true;
    }

    bool operator ()(AVFilterInOut* &filterInOut)
    {
        avfilter_inout_free(&filterInOut);
        return true;
    }
};


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
void array_to_container(const T* array, std::size_t nelemnts, Container &container)
{
    if (!array || nelemnts == 0)
        return;
    std::copy_n(array, array + nelemnts, std::back_inserter(container));
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

} // ::av

#endif // AV_UTILS_H
