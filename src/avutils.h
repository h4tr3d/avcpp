#pragma once

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>
#include <type_traits>

#if __has_include(<expected>)
#  include <expected>
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

// Use concepts if any
#define USE_CONCEPTS (__cplusplus > 201703L)

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

/////// av::expected

#if __cplusplus > 202002L && __cpp_concepts >= 202002L
template<typename E>
using unexpected = std::unexpected<E>;

template<typename T, typename E>
using expected = std::expected<T, E>;
#else

namespace __expected
{

template<typename _Tp>
struct _Guard
{
    static_assert( std::is_nothrow_move_constructible_v<_Tp> );

    constexpr explicit _Guard(_Tp& __x)
        : _M_guarded(__builtin_addressof(__x)), _M_tmp(std::move(__x)) // nothrow
    { std::destroy_at(_M_guarded); }

    constexpr ~_Guard()
    {
        if (_M_guarded) [[unlikely]]
            std::construct_at(_M_guarded, std::move(_M_tmp));
    }

    _Guard(const _Guard&) = delete;
    _Guard& operator=(const _Guard&) = delete;

    constexpr _Tp&&
    release() noexcept
    {
        _M_guarded = nullptr;
        return std::move(_M_tmp);
    }

private:
    _Tp* _M_guarded;
    _Tp _M_tmp;
};

// reinit-expected helper from [expected.object.assign]
template<typename _Tp, typename _Up, typename _Vp>
constexpr void __reinit(_Tp* __newval, _Up* __oldval, _Vp&& __arg)
    noexcept(std::is_nothrow_constructible_v<_Tp, _Vp>)
{
    if constexpr (std::is_nothrow_constructible_v<_Tp, _Vp>)
    {
        std::destroy_at(__oldval);
        std::construct_at(__newval, std::forward<_Vp>(__arg));
    }
    else if constexpr (std::is_nothrow_move_constructible_v<_Tp>)
    {
        _Tp __tmp(std::forward<_Vp>(__arg)); // might throw
        std::destroy_at(__oldval);
        std::construct_at(__newval, std::move(__tmp));
    }
    else
    {
        _Guard<_Up> __guard(*__oldval);
        std::construct_at(__newval, std::forward<_Vp>(__arg)); // might throw
        __guard.release();
    }
}
} // ::__expected


template<typename E>
class unexpected
{
    // static_assert( __expected::__can_be_unexpected<_Er> );
public:
    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&) = default;

    constexpr explicit
        unexpected(E&& __e)
        noexcept(std::is_nothrow_constructible_v<E>)
        : m_error(std::forward<E>(__e))
    { }

    constexpr unexpected& operator=(const unexpected&) = default;
    constexpr unexpected& operator=(unexpected&&) = default;

    [[nodiscard]]
    constexpr const E&
    error() const & noexcept { return m_error; }

    [[nodiscard]]
    constexpr E&
    error() & noexcept { return m_error; }

    [[nodiscard]]
    constexpr const E&&
    error() const && noexcept { return std::move(m_error); }

    [[nodiscard]]
    constexpr E&&
    error() && noexcept { return std::move(m_error); }

    constexpr void
    swap(unexpected& __other) noexcept(std::is_nothrow_swappable_v<E>)
    {
        using std::swap;
        swap(m_error, __other.m_error);
    }

    template<typename _Err>
    [[nodiscard]]
    friend constexpr bool
    operator==(const unexpected& __x, const unexpected<_Err>& __y)
    { return __x.m_error == __y.error(); }

    friend constexpr void
    swap(unexpected& __x, unexpected& __y) noexcept(noexcept(__x.swap(__y)))
    { __x.swap(__y); }

private:
    E m_error;
};

template <typename T, typename E>
class expected
{
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>)
        : m_value(), m_has_value(true) {}

#if 1
    expected(const expected&) = default;
#else
    constexpr
        expected(const expected& other)
        noexcept(std::conjunction_v<std::is_nothrow_copy_constructible<T>, std::is_nothrow_copy_constructible<E>>)
        : m_has_value(other.m_has_value)
    {
        if (m_has_value)
            std::construct_at(__builtin_addressof(m_value), other.m_value);
        else
            std::construct_at(__builtin_addressof(m_error), other.m_error);
    }
#endif

#if 1
    expected(expected&&) = default;
#else
    constexpr
        expected(expected&& other)
        noexcept(std::conjunction_v<std::is_nothrow_move_constructible<T>, std::is_nothrow_move_constructible<E>>)
        : m_has_value(other.m_has_value)
    {
        if (m_has_value)
            std::construct_at(__builtin_addressof(m_value), std::move(other).m_value);
        else
            std::construct_at(__builtin_addressof(m_error), std::move(other).m_error);
    }
#endif

    template<typename _Up = T>
    constexpr explicit(!std::is_convertible_v<_Up, T>)
        expected(_Up&& __v)
        noexcept(std::is_nothrow_constructible_v<T, _Up>)
        : m_value(std::forward<_Up>(__v)), m_has_value(true)
    { }

    template<typename _Gr = E>
    constexpr explicit(!std::is_convertible_v<const _Gr&, E>)
        expected(const unexpected<_Gr>& __u)
        noexcept(std::is_nothrow_constructible_v<E, const _Gr&>)
        : m_error(__u.error()), m_has_value(false)
    {}

    template<typename _Gr = E>
    constexpr explicit(!std::is_convertible_v<_Gr, E>)
        expected(unexpected<_Gr>&& __u)
        noexcept(std::is_nothrow_constructible_v<E, _Gr>)
        : m_error(std::move(__u).error()), m_has_value(false)
    {}

#if 1
    constexpr ~expected() = default;
#else
    constexpr ~expected()
    {
        if (m_has_value)
            std::destroy_at(__builtin_addressof(m_value));
        else
            std::destroy_at(__builtin_addressof(m_error));
    }
#endif

    // assignment

#if 0
    expected& operator=(const expected&) = delete;
#else
    constexpr expected&
    operator=(const expected& __x)
        noexcept(std::conjunction_v<std::is_nothrow_copy_constructible<T>,
                                                               std::is_nothrow_copy_constructible<E>,
                                                               std::is_nothrow_copy_assignable<T>,
                                                               std::is_nothrow_copy_assignable<E>>)
    {
        if (__x.m_has_value)
            this->_M_assign_val(__x._M_val);
        else
            this->_M_assign_unex(__x._M_unex);
        return *this;
    }
#endif

    constexpr expected&
    operator=(expected&& __x)
        noexcept(std::conjunction_v<std::is_nothrow_move_constructible<T>,
                                                          std::is_nothrow_move_constructible<E>,
                                                          std::is_nothrow_move_assignable<T>,
                                                          std::is_nothrow_move_assignable<E>>)
    {
        if (__x.m_has_value)
            _M_assign_val(std::move(__x.m_value));
        else
            _M_assign_unex(std::move(__x.m_error));
        return *this;
    }

    template<typename _Up = T>
    constexpr expected&
    operator=(_Up&& __v)
    {
        _M_assign_val(std::forward<_Up>(__v));
        return *this;
    }

    template<typename _Gr>
    constexpr expected&
    operator=(const unexpected<_Gr>& __e)
    {
        _M_assign_unex(__e.error());
        return *this;
    }

    template<typename _Gr>
    constexpr expected&
    operator=(unexpected<_Gr>&& __e)
    {
        _M_assign_unex(std::move(__e).error());
        return *this;
    }


    // swap
    constexpr void
    swap(expected& __x)
        noexcept(std::conjunction_v<std::is_nothrow_move_constructible<T>,
                         std::is_nothrow_move_constructible<E>,
                         std::is_nothrow_swappable<T&>,
                         std::is_nothrow_swappable<E&>>)
    {
        if (m_has_value)
        {
            if (__x.m_has_value)
            {
                using std::swap;
                swap(m_value, __x.m_value);
            }
            else
                this->_M_swap_val_unex(__x);
        }
        else
        {
            if (__x.m_has_value)
                __x._M_swap_val_unex(*this);
            else
            {
                using std::swap;
                swap(m_error, __x.m_error);
            }
        }
    }

    // observers

    [[nodiscard]]
    constexpr const T*
    operator->() const noexcept
    {
        __glibcxx_assert(m_has_value);
        return __builtin_addressof(m_value);
    }

    [[nodiscard]]
    constexpr T*
    operator->() noexcept
    {
        __glibcxx_assert(m_has_value);
        return __builtin_addressof(m_value);
    }

    [[nodiscard]]
    constexpr const T&
    operator*() const & noexcept
    {
        __glibcxx_assert(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr T&
    operator*() & noexcept
    {
        __glibcxx_assert(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr const T&&
    operator*() const && noexcept
    {
        __glibcxx_assert(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr T&&
    operator*() && noexcept
    {
        __glibcxx_assert(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr explicit
    operator bool() const noexcept { return m_has_value; }

    [[nodiscard]]
    constexpr bool has_value() const noexcept { return m_has_value; }

    constexpr const T&
    value() const &
    {
        if (m_has_value) [[likely]]
            return m_value;
        // --htrd: implement me
        // _GLIBCXX_THROW_OR_ABORT(std::bad_expected_access<E>(_M_unex));
    }

    constexpr T&
    value() &
    {
        if (m_has_value) [[likely]]
            return m_value;
        // --htrd: implement me
        // const auto& __unex = m_error;
        // _GLIBCXX_THROW_OR_ABORT(bad_expected_access<_Er>(__unex));
    }

    constexpr const T&&
    value() const &&
    {
        if (m_has_value) [[likely]]
            return std::move(m_value);
        // --htrd: implement me
        //_GLIBCXX_THROW_OR_ABORT(bad_expected_access<_Er>(std::move(_M_unex)));
    }

    constexpr T&&
    value() &&
    {
        if (m_has_value) [[likely]]
            return std::move(m_value);
        // --htrd: implement me
        //_GLIBCXX_THROW_OR_ABORT(bad_expected_access<_Er>(std::move(_M_unex)));
    }

    constexpr const E&
    error() const & noexcept
    {
        __glibcxx_assert(!m_has_value);
        return m_error;
    }

    constexpr E&
    error() & noexcept
    {
        __glibcxx_assert(!m_has_value);
        return m_error;
    }

    constexpr const E&&
    error() const && noexcept
    {
        __glibcxx_assert(!m_has_value);
        return std::move(m_error);
    }

    constexpr E&&
    error() && noexcept
    {
        __glibcxx_assert(!m_has_value);
        return std::move(m_error);
    }


private:
    template<typename _Vp>
    constexpr void
    _M_assign_val(_Vp&& __v)
    {
        if (m_has_value)
            m_value = std::forward<_Vp>(__v);
        else
        {
            __expected::__reinit(__builtin_addressof(m_value),
                                 __builtin_addressof(m_error),
                                 std::forward<_Vp>(__v));
            m_has_value = true;
        }
    }

    template<typename _Vp>
    constexpr void
    _M_assign_unex(_Vp&& __v)
    {
        if (m_has_value)
        {
            __expected::__reinit(__builtin_addressof(m_error),
                                 __builtin_addressof(m_value),
                                 std::forward<_Vp>(__v));
            m_has_value = false;
        }
        else
            m_error = std::forward<_Vp>(__v);
    }

    // Swap two expected objects when only one has a value.
    // Precondition: this->_M_has_value && !__rhs._M_has_value
    constexpr void
    _M_swap_val_unex(expected& __rhs)
        noexcept(std::conjunction_v<std::is_nothrow_move_constructible<E>,
                                                                  std::is_nothrow_move_constructible<T>>)
    {
        if constexpr (std::is_nothrow_move_constructible_v<E>)
        {
            __expected::_Guard<E> __guard(__rhs.m_error);
            std::construct_at(__builtin_addressof(__rhs.m_value),
                              std::move(m_value)); // might throw
            __rhs.m_has_value = true;
            std::destroy_at(__builtin_addressof(m_value));
            std::construct_at(__builtin_addressof(m_error),
                              __guard.release());
            m_has_value = false;
        }
        else
        {
            __expected::_Guard<T> __guard(m_value);
            std::construct_at(__builtin_addressof(m_error),
                              std::move(__rhs.m_error)); // might throw
            m_has_value = false;
            std::destroy_at(__builtin_addressof(__rhs.m_error));
            std::construct_at(__builtin_addressof(__rhs.m_value),
                              __guard.release());
            __rhs.m_has_value = true;
        }
    }

private:
    union {
        value_type m_value;
        error_type m_error;
    };
    bool m_has_value;
};

#endif // __cplusplus > 202002L && __cpp_concepts >= 202002L

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

} // ::av

