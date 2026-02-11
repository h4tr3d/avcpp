#pragma once

#include "avconfig.h"

#include <ranges>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>
#include <type_traits>

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

#if AVCPP_CXX_STANDARD >= 20

/**
 * Sentinel policy that defines well-known range end pointer
 */
template<class T>
struct SentinelEndPolicy
{
    using value_type = std::remove_reference_t<T>;
    value_type* end{};
    constexpr bool isEnd(T* it) const noexcept {
        return it == end;
    }
};

/**
 * Sentinel policy that defines well-known sequence end marker, like '\0' at the C-string end or special value like NULL
 */
template<class T>
struct SentinelUntilPolicy
{
    using value_type = std::remove_cvref_t<T>;
    value_type value{};
    constexpr bool isEnd(T* it) const noexcept {
        return it == nullptr || *it == value;
    }
};


/**
 * Policy based sentinel marker
 */
template<class T, class Policy>
struct ArraySentinel
{
    Policy policy;
    friend constexpr bool operator==(T* it, const ArraySentinel& s) noexcept {
        return s.policy.isEnd(it);
    }
};

template<class T, class U, bool UseProxy>
struct ReferenceSelector;

template<class T, class U>
struct ReferenceSelector<T, U, true>
{
    using Type = T&;
};

template<class T, class U>
struct ReferenceSelector<T, U, false>
{
    using Type = U;
};

/**
 * Wrapped array iterator.
 *
 * Represents any element of the T* array as light-weight wrapper U
 *
 */
template<typename T, typename U>
struct ArrayIterator
{
    static constexpr bool UseProxy = !std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    using difference_type   = std::ptrdiff_t;
    using value_type        = std::remove_reference_t<U>;
    using pointer           = std::remove_reference_t<T>*;  // or also value_type*

    using reference         = typename ReferenceSelector<T, U, !UseProxy>::Type;
    using iterator_category =
        std::conditional_t<
            UseProxy,
            std::random_access_iterator_tag,
            std::contiguous_iterator_tag>;

    ArrayIterator() noexcept = default; // semiregular
    ArrayIterator(pointer ptr) noexcept : m_ptr(ptr) {}

    reference operator*() const noexcept {
        if constexpr (UseProxy)
            return reference{*m_ptr};
        else
            return *m_ptr;
    }

    ArrayIterator& operator++()      noexcept { m_ptr++; return *this; }
    ArrayIterator  operator++(int)   noexcept { ArrayIterator tmp = *this; ++(*this); return tmp; }

    ArrayIterator& operator--()      noexcept { m_ptr--; return *this; }
    ArrayIterator  operator--(int)   noexcept { ArrayIterator tmp = *this; --(*this); return tmp; }

    ArrayIterator& operator+=(int n) noexcept { m_ptr+=n; return *this; }
    ArrayIterator& operator-=(int n) noexcept { m_ptr-=n; return *this; }

    // i[n]
    reference operator[](int n) noexcept {
        if constexpr (UseProxy)
            return reference{m_ptr[n]};
        else
            return m_ptr[n];
    }
    const reference operator[](int n) const noexcept {
        if constexpr (UseProxy)
            return reference{m_ptr[n]};
        else
            return m_ptr[n];
    }
    //reference operator&() noexcept { return }
    // a + n
    // n + a
    friend ArrayIterator operator+(const ArrayIterator& a, int n) noexcept { return {a.m_ptr + n}; }
    friend ArrayIterator operator+(int n, const ArrayIterator& a) noexcept { return {a.m_ptr + n}; }
    // i - n
    friend ArrayIterator operator-(const ArrayIterator& a, int n) noexcept { return {a.m_ptr - n}; }
    // b - a
    friend difference_type operator-(const ArrayIterator& b, const ArrayIterator& a) noexcept { return b.m_ptr - a.m_ptr; }

    // a<=>b
    friend std::strong_ordering operator<=>(const ArrayIterator& a, const ArrayIterator& b) noexcept {
        return a.m_ptr == b.m_ptr ? std::strong_ordering::equal
               : a.m_ptr < b.m_ptr  ? std::strong_ordering::less
                                   : std::strong_ordering::greater;
    }
    friend bool operator== (const ArrayIterator& a, const ArrayIterator& b) { return a.m_ptr == b.m_ptr; };
    friend bool operator!= (const ArrayIterator& a, const ArrayIterator& b) { return !(a == b); };

    //
    // Sentinels
    //
    friend bool operator==(ArrayIterator it, std::default_sentinel_t) = delete;

    friend bool operator==(ArrayIterator it, auto sentinel) { return it.m_ptr == sentinel; }
    friend bool operator!=(ArrayIterator it, auto sentinel) { return it.m_ptr != sentinel; }
    friend bool operator==(auto sentinel, ArrayIterator it) { return it.m_ptr == sentinel; }
    friend bool operator!=(auto sentinel, ArrayIterator it) { return it.m_ptr != sentinel; }

private:
    pointer m_ptr{};
};

/**
 * C-array (FFmpeg) wrapper witch can iterate over collection and represents any element via light-weight view U
 *
 * Policy maybe std::size_t that just defines input array size
 */
template<typename T, typename U, class Policy>
class ArrayView : public std::ranges::view_interface<ArrayView<T, U, Policy>>
{
public:
    constexpr ArrayView(T *ptr, Policy policy)
        : m_ptr(ptr), m_policy(std::move(policy))
    {}

    auto constexpr begin() const noexcept
    {
        return ArrayIterator<T, U>{m_ptr};
    }

    auto constexpr end() const noexcept
    {
        // special case to allow back()/size()/etc method for well-defined sizes
        if constexpr (std::is_same_v<Policy, std::size_t>) {
            return ArrayIterator<T, U>{m_ptr + m_policy};
        } else {
            return ArraySentinel<T, Policy>{m_policy};
        }
    }

private:
    T *m_ptr{};
    Policy m_policy{};
};

/**
 * Helper to create array view with well-known size
 * @param ptr
 * @param size
 */
template<class U>
auto make_array_view_size(auto* ptr, std::size_t size)
{
    using T = std::remove_reference_t<decltype(*ptr)>;
    return ArrayView<T, U, std::size_t>(ptr, size);
}

/**
 * Helper to create array view with end marker
 * @param ptr
 * @param value
 */
template<class U>
auto make_array_view_until(auto* ptr, auto value)
{
    using T = std::remove_reference_t<decltype(*ptr)>;
    return ArrayView<T, U, SentinelUntilPolicy<T>>(ptr, SentinelUntilPolicy<T>{value});
}

/**
 * Select more approptiate value from given value list. Useful for AVCodec::supported_framerates,
 * AVCodec::pix_fmts and so on.
 *
 * T - type of value and (by default) list elements
 * L - type of list elements, range compatible
 *
 * If T and elements of L has different types, T must be have ctor from L-element.
 *
 * @param value           value to set
 * @param list            list of allowed values
 * @param endOfListValue  end of list value, like
 * @return value if list null or if it present in list, or more appropriate value from list
 */
template<typename T, typename L>
    requires std::ranges::range<L> &&
    requires(L& r) {
        std::ranges::empty(r);
    }
T guessValue(const T& value, L list)
{
    if (list.empty())
        return value;

    T best = value;
    T bestDistance = std::numeric_limits<T>::max();

    for (auto&& cur : list) {
        auto const distance = std::max(cur, value) - std::min(cur, value);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = cur;
        }
    }

    return best;
}

#endif // AVCPP_CXX_STANDARD

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

    T best = value;
    T bestDistance = std::numeric_limits<T>::max();

    for (const L * ptr = list; !endListComparator(*ptr); ++ptr) {
        auto const cur = *ptr;
        auto const distance = std::max(cur, value) - std::min(cur, value);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = cur;
        }
    }

    return best;
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

} // ::av

