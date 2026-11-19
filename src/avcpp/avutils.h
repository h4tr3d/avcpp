#pragma once

/**
 * Common helpers: comparisons, value lookup, non-copyable base, string utilities.
 *
 * @file
 * @ingroup core
 */

#include "avcpp/avconfig.h"

#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <limits>

#if AVCPP_CXX_STANDARD >= 20
#  include <span>
#endif

#include "ffmpeg.h"
#include "avtime.h"

extern "C" {
#include <libavutil/avstring.h>
#include <libavutil/mem.h>
}

#if AVCPP_HAS_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
}
#endif

//
// Functions
//
namespace av {

// Basic FFmpeg constants
/**
 * Timestamp value meaning "no timestamp".
 *
 * The avcpp spelling of `AV_NOPTS_VALUE`; compare timestamps against it instead
 * of hard-coding the integer.
 */
constexpr auto NoPts = static_cast<int64_t>(AV_NOPTS_VALUE);
/**
 * Denominator of the global time base used for container-level timestamps.
 */
constexpr auto TimeBase = static_cast<int>(AV_TIME_BASE);
/**
 * The global time base as a rational value.
 */
constexpr auto TimeBaseQ = AVRational{1, AV_TIME_BASE};


/**
 * Convert a value through its stream representation.
 *
 * @tparam R the type to produce
 * @tparam T the type to convert from
 *
 * @param v the value to convert
 *
 * @return the converted value; the result is default-constructed when the
 *         conversion fails
 */
template<typename R, typename T>
R lexical_cast(const T& v)
{
    R result;
    std::stringstream ss;
    ss << v;
    ss >> result;
    return result;
}

/**
 * Base class that deletes copying and assignment.
 *
 * Used by the wrappers that own a resource, so a copy cannot silently produce
 * two owners. Moves are left to the derived class.
 */
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
/**
 * Old name of set_logging_level(), kept for source compatibility.
 *
 * @deprecated Use set_logging_level().
 */
#define setFFmpegLoggingLevel set_logging_level

/**
 * @brief dump_binary_buffer
 * Dump binary buffer to std out in HEX view
 * @param buffer        pointer to buffer start
 * @param buffer_size   buffer size
 * @param width         output width in hex values (real text screen with is: sw = width * 3 - 1)
 */
[[deprecated("Use av::hex_dump()")]]
void dumpBinaryBuffer(uint8_t *buffer, int buffer_size, int width = 16);


/**
 * C++ verstion of the av_err2str()
 * @param error - error code to convert to string
 * @return string representation of error code
 */
std::string error2string(int error);

/**
 * Wraps a nullable C string into a string_view without copying.
 * @param value
 * @return a view of the string, or an empty view for null
 */
inline std::string_view wrap2sview(const char *value) noexcept
{
    return value ? std::string_view(value) : std::string_view();
}

}




//
// Classes
//
namespace av {

/**
 * Deleter that does nothing.
 *
 * Used when a pointer is wrapped in a smart pointer but must not be freed,
 * for example for FFmpeg-owned static structures.
 */
struct EmptyDeleter
{
    /**
     * Accept the pointer and do nothing with it.
     */
    void operator()(void *) {}
};

/**
 * @brief The AvDeleter struct
 * Unified delete functor for variois FFMPEG/libavformat/libavcodec and so on resource allocators.
 * Can be used with shared_ptr<> and so on.
 */
namespace v1 {
/**
 * Free functors for the FFmpeg resource types.
 *
 * Each overload accepts a pointer by reference, frees the object with the
 * matching FFmpeg call and resets the pointer, so the deleter can be used with
 * std::unique_ptr or std::shared_ptr without leaving a dangling value.
 */
struct AvDeleter
{
    /**
     * Free a swscale context.
     *
     * @param swsContext the pointer to free; reset to null
     *
     * @return `true` when the context was freed
     */
    bool operator() (struct SwsContext* &swsContext);
    /**
     * Free a codec context.
     *
     * @param codecContext the pointer to free; reset to null
     *
     * @return `true` when the context was freed
     */
    bool operator() (struct AVCodecContext* &codecContext);
#if AVCPP_HAS_AVFORMAT
    /**
     * Free an output format.
     *
     * @param format the pointer to free; reset to null
     *
     * @return `true` when the format was freed
     */
    bool operator() (struct AVOutputFormat* &format);
    /**
     * Close and free a format context.
     *
     * @param formatContext the pointer to free; reset to null
     *
     * @return `true` when the context was freed
     */
    bool operator() (struct AVFormatContext* &formatContext);
#endif // if AVCPP_HAS_AVFORMAT
    /**
     * Free a frame and its buffers.
     *
     * @param frame the pointer to free; reset to null
     *
     * @return `true` when the frame was freed
     */
    bool operator() (struct AVFrame* &frame);
    /**
     * Free a packet and its buffer.
     *
     * @param packet the pointer to free; reset to null
     *
     * @return `true` when the packet was freed
     */
    bool operator() (struct AVPacket* &packet);
    /**
     * Free a dictionary and all its entries.
     *
     * @param dictionary the pointer to free; reset to null
     *
     * @return `true` when the dictionary was freed
     */
    bool operator() (struct AVDictionary* &dictionary);
#if AVCPP_HAS_AVFILTER
    /**
     * Free a filter input/output list.
     *
     * @param filterInOut the pointer to free; reset to null
     *
     * @return `true` when the list was freed
     */
    bool operator ()(struct AVFilterInOut* &filterInOut);
#endif // if AVCPP_HAS_AVFILTER
    /**
     * Drop one reference to a buffer.
     *
     * @param bufferRef the pointer to unreference; reset to null
     *
     * @return `true` when a reference was dropped
     */
    bool operator() (struct AVBufferRef* &bufferRef);
};
} // ::v1

inline namespace v2 {
/**
 * Template facade over the per-type deleters of namespace v1.
 *
 * Lets a single std::unique_ptr<T, SmartDeleter> be written without naming the
 * concrete FFmpeg type.
 */
struct SmartDeleter
{
    /**
     * Free an FFmpeg object of type T.
     *
     * @tparam T the FFmpeg type
     *
     * @param ptr the object to free
     *
     * @return whatever the matching v1 deleter reports
     */
    template<typename T>
    bool operator() (T *ptr) {
        return v1::AvDeleter()(ptr);
    }
};
}

/**
 * Allocate memory with the FFmpeg allocator.
 *
 * @tparam T the element type
 *
 * @param size size in bytes
 *
 * @return an owning pointer freed with av_free(), or a null one on failure
 */
template<typename T>
std::unique_ptr<T, void(*)(void*)> malloc(size_t size)
{
    return {static_cast<T*>(av_malloc(size)), av_free};
}

/**
 * Allocate zeroed memory with the FFmpeg allocator.
 *
 * @tparam T the element type
 *
 * @param size size in bytes
 *
 * @return an owning pointer freed with av_free(), or a null one on failure
 */
template<typename T>
std::unique_ptr<T, void(*)(void*)> mallocz(size_t size)
{
    return {static_cast<T*>(av_mallocz(size)), av_free};
}

/**
 * Copy a memory block using the FFmpeg allocator.
 *
 * @tparam T the element type
 *
 * @param p    source block; may be null
 * @param size number of bytes to copy
 *
 * @return an owning pointer freed with av_free(), or a null one on failure
 */
template<typename T>
std::unique_ptr<T, void(*)(void*)> memdup(const void *p, size_t size)
{
    return {static_cast<T*>(av_memdup(p, size)), av_free};
}

/**
 * Duplicates a string using the FFmpeg allocator.
 * Returns nullptr for an empty input.
 * @param value
 * @return an owning pointer freed with av_free(), or a null one for an empty input
 */
inline std::unique_ptr<char, void(*)(void*)> strdup(std::string_view value)
{
    if (value.empty())
        return {nullptr, av_free};

    return {av_strndup(value.data(), value.size()), av_free};
}

/**
 * Frees a buffer allocated by the FFmpeg allocator and resets the pointer.
 * The pointer must come from av_malloc() or a compatible FFmpeg allocation.
 * @param ptr
 */
template<typename T>
void freep(T *&ptr) noexcept
{
    ::av_freep(&ptr);
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
    /**
     * Run a callback when the guard leaves scope.
     *
     * @tparam Proc callable type
     *
     * @param proc the callback to invoke; it is copied into the guard
     */
    template<typename Proc>
    ScopeOutAction(const Proc& proc)
        : m_proc(proc)
    {}

    /**
     * Run a callback when the guard leaves scope, moving it in.
     *
     * @tparam Proc callable type
     *
     * @param proc the callback to invoke; it is moved into the guard
     */
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
    /// Type of the range elements.
    using value_type = std::remove_reference_t<T>;
        /// Pointer that marks the end of the range.
    value_type* end{};
        /**
         * Whether the iterator reached the end pointer.
         *
         * @param it the iterator to test
         *
         * @return `true` when it equals the stored end
         */
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
    /// Type of the range elements.
    using value_type = std::remove_cvref_t<T>;
        /// Value that terminates the sequence, for example '\0' or a null pointer.
    value_type value{};
        /**
         * Whether the iterator reached the terminating value.
         *
         * @param it the iterator to test
         *
         * @return `true` when it is null or points at the terminator
         */
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
        /// Policy that decides where the range ends.
    Policy policy;
    /**
     * Compare an iterator with the sentinel.
     *
     * @param it the iterator
     * @param s  the sentinel
     *
     * @return `true` when the policy reports the end of the range
     */
    friend constexpr bool operator==(T* it, const ArraySentinel& s) noexcept {
        return s.policy.isEnd(it);
    }
};

template<class T, class U, bool UseProxy>
struct ReferenceSelector;

/**
 * Selects a reference as the dereference result.
 *
 * @tparam T the array element type
 * @tparam U the view type
 */
template<class T, class U>
struct ReferenceSelector<T, U, true>
{
    /// Dereference result when no proxy is needed.
    using Type = T&;
};

/**
 * Selects a value as the dereference result.
 *
 * @tparam T the array element type
 * @tparam U the view type
 */
template<class T, class U>
struct ReferenceSelector<T, U, false>
{
    /// Dereference result when a proxy view is produced.
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
    /// Whether dereferencing must produce a proxy view instead of a reference.
    static constexpr bool UseProxy = !std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

        /// Distance type required by the iterator traits.
    using difference_type   = std::ptrdiff_t;
        /// Type produced by dereferencing.
    using value_type        = std::remove_reference_t<U>;
        /// Pointer type of the underlying array.
    using pointer           = std::remove_reference_t<T>*;  // or also value_type*

        /// Dereference result: a proxy wrapper or a plain reference.
    using reference         = typename ReferenceSelector<T, U, !UseProxy>::Type;
        /// Iterator category: random access for proxies, contiguous otherwise.
    using iterator_category =
        std::conditional_t<
            UseProxy,
            std::random_access_iterator_tag,
            std::contiguous_iterator_tag>;

    ArrayIterator() noexcept = default; // semiregular
        /**
         * Point the iterator at an array element.
         *
         * @param ptr position in the array
         */
    ArrayIterator(pointer ptr) noexcept : m_ptr(ptr) {}

        /**
         * Element at the current position.
         *
         * @return a wrapped view or a reference, depending on the proxy mode
         */
    reference operator*() const noexcept {
        if constexpr (UseProxy)
            return reference{*m_ptr};
        else
            return *m_ptr;
    }

        /**
         * Advance to the next element.
         *
         * @return this iterator
         */
    ArrayIterator& operator++()      noexcept { m_ptr++; return *this; }
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
    ArrayIterator  operator++(int)   noexcept { ArrayIterator tmp = *this; ++(*this); return tmp; }

        /**
         * Step back to the previous element.
         *
         * @return this iterator
         */
    ArrayIterator& operator--()      noexcept { m_ptr--; return *this; }
        /**
         * Step back, returning the previous position.
         *
         * @return a copy of the iterator before stepping back
         */
    ArrayIterator  operator--(int)   noexcept { ArrayIterator tmp = *this; --(*this); return tmp; }

        /**
         * Advance by several elements.
         *
         * @param n number of elements to skip; may be negative
         *
         * @return this iterator
         */
    ArrayIterator& operator+=(int n) noexcept { m_ptr+=n; return *this; }
        /**
         * Step back by several elements.
         *
         * @param n number of elements to go back; may be negative
         *
         * @return this iterator
         */
    ArrayIterator& operator-=(int n) noexcept { m_ptr-=n; return *this; }

    // i[n]
        /**
         * Element at an offset from the current position.
         *
         * @param n offset in elements
         *
         * @return the element, wrapped or referenced
         */
    reference operator[](int n) noexcept {
        if constexpr (UseProxy)
            return reference{m_ptr[n]};
        else
            return m_ptr[n];
    }
        /**
         * Element at an offset, read-only.
         *
         * @param n offset in elements
         *
         * @return the element, wrapped or referenced
         */
    const reference operator[](int n) const noexcept {
        if constexpr (UseProxy)
            return reference{m_ptr[n]};
        else
            return m_ptr[n];
    }
    //reference operator&() noexcept { return }
    // a + n
    // n + a
    /**
     * Advance an iterator.
     *
     * @param a the iterator
     * @param n number of elements to skip
     *
     * @return the advanced iterator
     */
    friend ArrayIterator operator+(const ArrayIterator& a, int n) noexcept { return {a.m_ptr + n}; }
    /**
     * Advance an iterator.
     *
     * @param n number of elements to skip
     * @param a the iterator
     *
     * @return the advanced iterator
     */
    friend ArrayIterator operator+(int n, const ArrayIterator& a) noexcept { return {a.m_ptr + n}; }
    // i - n
    /**
     * Step an iterator back.
     *
     * @param a the iterator
     * @param n number of elements to go back
     *
     * @return the resulting iterator
     */
    friend ArrayIterator operator-(const ArrayIterator& a, int n) noexcept { return {a.m_ptr - n}; }
    // b - a
    /**
     * Distance between two iterators.
     *
     * @param b the later iterator
     * @param a the earlier iterator
     *
     * @return the difference in elements
     */
    friend difference_type operator-(const ArrayIterator& b, const ArrayIterator& a) noexcept { return b.m_ptr - a.m_ptr; }

    // a<=>b
    /**
     * Order two iterators.
     *
     * @param a the left iterator
     * @param b the right iterator
     *
     * @return the ordering of their positions
     */
    friend std::strong_ordering operator<=>(const ArrayIterator& a, const ArrayIterator& b) noexcept {
        return a.m_ptr == b.m_ptr ? std::strong_ordering::equal
               : a.m_ptr < b.m_ptr  ? std::strong_ordering::less
                                   : std::strong_ordering::greater;
    }
    /**
     * Compare two iterators.
     *
     * @param a the left iterator
     * @param b the right iterator
     *
     * @return `true` when both point at the same element
     */
    friend bool operator== (const ArrayIterator& a, const ArrayIterator& b) { return a.m_ptr == b.m_ptr; };
    /**
     * Compare two iterators.
     *
     * @param a the left iterator
     * @param b the right iterator
     *
     * @return `true` when they point at different elements
     */
    friend bool operator!= (const ArrayIterator& a, const ArrayIterator& b) { return !(a == b); };

    //
    // Sentinels
    //
    friend bool operator==(ArrayIterator it, std::default_sentinel_t) = delete;

    /**
     * Compare an iterator with a sentinel.
     *
     * @param it       the iterator
     * @param sentinel the sentinel
     *
     * @return `true` when the policy reports the end of the range
     */
    template<class Policy>
    friend constexpr bool operator==(ArrayIterator it, const ArraySentinel<T, Policy>& sentinel)
    { return sentinel.policy.isEnd(it.m_ptr); }

    /**
     * Compare an iterator with a sentinel.
     *
     * @param it       the iterator
     * @param sentinel the sentinel
     *
     * @return `true` while the policy does not report the end
     */
    template<class Policy>
    friend constexpr bool operator!=(ArrayIterator it, const ArraySentinel<T, Policy>& sentinel)
    { return !sentinel.policy.isEnd(it.m_ptr); }

    /**
     * Compare a sentinel with an iterator.
     *
     * @param sentinel the sentinel
     * @param it       the iterator
     *
     * @return `true` when the policy reports the end of the range
     */
    template<class Policy>
    friend constexpr bool operator==(const ArraySentinel<T, Policy>& sentinel, ArrayIterator it)
    { return sentinel.policy.isEnd(it.m_ptr); }

    /**
     * Compare a sentinel with an iterator.
     *
     * @param sentinel the sentinel
     * @param it       the iterator
     *
     * @return `true` while the policy does not report the end
     */
    template<class Policy>
    friend constexpr bool operator!=(const ArraySentinel<T, Policy>& sentinel, ArrayIterator it)
    { return !sentinel.policy.isEnd(it.m_ptr); }

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
    constexpr ArrayView() = default;
        /**
         * Wrap an array with a policy that defines where it ends.
         *
         * @param ptr    first element; may be null
         * @param policy either an element count or a sentinel policy
         *
         * @note Non-owning: the array must outlive the view.
         */
    constexpr ArrayView(T *ptr, Policy policy)
        : m_ptr(ptr), m_policy(std::move(policy))
    {}

        /**
         * First element.
         *
         * @return an iterator at the beginning of the range
         */
    auto constexpr begin() const noexcept
    {
        return ArrayIterator<T, U>{m_ptr};
    }

        /**
         * End of the range.
         *
         * @return an iterator or a sentinel, depending on the policy
         */
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
 * @return a non-owning view over the array
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
 * @return a non-owning view that ends at the first element equal to the value
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
    T bestDistance = (std::numeric_limits<T>::max)();

    for (auto&& cur : list) {
        auto const distance = (std::max)(cur, value) - (std::min)(cur, value);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = cur;
        }
    }

    return best;
}

#endif // AVCPP_CXX_STANDARD

/**
 * Predicate that compares an element with a stored value.
 *
 * Used as the "end of list" marker by the pointer overload of guessValue().
 */
template<typename T>
struct EqualComparator
{
        /**
         * Remember the value to compare against.
         *
         * @param value the value that ends the list
         */
    EqualComparator(const T& value)
        : value(value)
    {}

        /**
         * Compare an element with the stored value.
         *
         * @param value the element to test
         *
         * @return `true` when the element equals the stored value
         */
    bool operator() (const T& value) const
    {
        if (this->value == value)
            return true;

        return false;
    }

    /// The value that terminates the list.
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
 * @param endListComparator  predicate that tells whether the element ends the list
 * @return value if list null or if it present in list, or more appropriate value from list
 */
template<typename T, typename L, typename C>
T guessValue(const T& value, const L * list, C endListComparator)
{
    if (!list)
        return value;

    T best = value;
    T bestDistance = (std::numeric_limits<T>::max)();

    for (const L * ptr = list; !endListComparator(*ptr); ++ptr) {
        auto const cur = *ptr;
        auto const distance = (std::max)(cur, value) - (std::min)(cur, value);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = cur;
        }
    }

    return best;
}


/**
 * Append a fixed number of array elements to a container.
 *
 * @param array     the source array; null is ignored
 * @param nelements number of elements to copy
 * @param container destination container
 */
template<typename T, typename Container>
void array_to_container(const T* array, std::size_t nelements, Container &container)
{
    if (!array || nelements == 0)
        return;
    std::copy_n(array, array + nelements, std::back_inserter(container));
}

/**
 * Append converted array elements to a container.
 *
 * @param array     the source array; null is ignored
 * @param nelements number of elements to copy
 * @param container destination container
 * @param convert   callable applied to each element before it is appended
 */
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

/**
 * Append elements until the predicate reports the end of the list.
 *
 * @param array     the source array; null is ignored
 * @param container destination container
 * @param isEnd     predicate that marks the terminator, evaluated before each
 *                  element is appended
 */
template<typename T, typename Container, typename Compare>
void array_to_container(const T* array, Container &container, Compare isEnd)
{
    if (!array)
        return;
    T value;
    while (!isEnd(value = *array++))
        container.push_back(value);
}

/**
 * Append converted elements until the predicate reports the end of the list.
 *
 * @param array     the source array; null is ignored
 * @param container destination container
 * @param isEnd     predicate that marks the terminator
 * @param convert   callable applied to each element before it is appended
 */
template<typename T, typename Container, typename Compare, typename Callable>
void array_to_container(const T* array, Container &container, Compare isEnd, Callable convert)
{
    if (!array)
        return;
    T value;
    while (!isEnd(value = *array++))
        container.push_back(convert(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Allow to use without AVFORMAT library
//
/**
 * Write a hexadecimal dump of a buffer to a C stream.
 *
 * @param f    destination stream
 * @param buf  buffer to dump; null is ignored
 * @param size number of bytes to dump
 */
void hex_dump(FILE *f, const uint8_t *buf, std::size_t size);
/**
 * Write a hexadecimal dump of a buffer through the FFmpeg log system.
 *
 * @param avcl  log context, usually a codec or format context
 * @param level an `AV_LOG_*` level
 * @param buf   buffer to dump; null is ignored
 * @param size  number of bytes to dump
 */
void hex_dump_log(void *avcl, int level, const uint8_t *buf, std::size_t size);

#if AVCPP_CXX_STANDARD >= 20
/**
 * Write a hexadecimal dump of a span to a C stream.
 *
 * @param f   destination stream
 * @param buf bytes to dump
 */
void hex_dump(FILE *f, std::span<const uint8_t> buf);
/**
 * Write a hexadecimal dump of a span through the FFmpeg log system.
 *
 * @param avcl  log context, usually a codec or format context
 * @param level an `AV_LOG_*` level
 * @param buf   bytes to dump
 */
void hex_dump_log(void *avcl, int level, std::span<const uint8_t> buf);
#endif // if AVCPP_CXX_STANDARD >= 20

} // ::av

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cpp_lib_format
#    include <format>

namespace av {
template <typename B>
concept has_name_method_with_ec = requires(const B& type, std::error_code ec) {
    { type.name(ec) } -> std::convertible_to<std::string_view>;
};

template <typename B>
concept has_name_method_without_ec = requires(const B& type) {
    { type.name() } -> std::convertible_to<std::string_view>;
};

template <typename B>
concept has_long_name_method_with_ec = requires(const B& type, std::error_code ec) {
    { type.longName(ec) } -> std::convertible_to<std::string_view>;
};

template <typename B>
concept has_long_name_method_without_ec = requires(const B& type) {
    { type.longName() } -> std::convertible_to<std::string_view>;
};
} // ::av

template <class T, class CharT>
    requires av::has_name_method_with_ec<T> || av::has_name_method_without_ec<T>
struct std::formatter<T, CharT>
{
    bool longName = false;

    template<typename ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        auto it = ctx.begin();
        if constexpr (requires { requires av::has_long_name_method_with_ec<T> || av::has_long_name_method_without_ec<T>; }) {
            if (it == ctx.end())
                return it;
            if (*it == 'l') {
                longName = true;
                ++it;
            }
            if (it != ctx.end() && *it != '}')
                throw std::format_error("Invalid format args");
        }
        return it;
    }

    template<typename ParseContext>
    auto format(const T& value, ParseContext& ctx) const
    {
        if (longName) {
            if constexpr (requires { requires av::has_long_name_method_with_ec<T>; }) {
                std::error_code dummy;
                return std::format_to(ctx.out(), "{}", value.longName(dummy));
            } else if constexpr (requires { requires av::has_long_name_method_without_ec<T>; }) {
                return std::format_to(ctx.out(), "{}", value.longName());
            }
        } else {
            if constexpr (requires { requires av::has_name_method_with_ec<T>; }) {
                std::error_code dummy;
                return std::format_to(ctx.out(), "{}", value.name(dummy));
            } else {
                return std::format_to(ctx.out(), "{}", value.name());
            }
        }
        return ctx.out();
    }
};
#endif
