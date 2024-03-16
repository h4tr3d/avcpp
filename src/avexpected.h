#pragma once

/**
 * @file avexpected.h
 *
 * std::expected/std::unexpected implementation for non C++23 enabled compilers.
 *
 * On C++23 ready compilers av::expected and av::unexpected directly mapped to the std::expected and std::unexpected
 *
 */

#include <cassert>
#include <memory>
#include <exception>
#include <type_traits>

#if __has_include(<expected>)
#  include <expected>
#endif

namespace av {

#if __cplusplus > 202002L && __cpp_concepts >= 202002L
template<typename E>
using unexpected = std::unexpected<E>;

template<typename T, typename E>
using expected = std::expected<T, E>;

template<typename E>
using bad_expected_access = std::bad_expected_access<E>;

#else

namespace details {

#if __cplusplus < 202002L
template<typename _Tp, typename... _Args>
constexpr auto
construct_at(_Tp* __location, _Args&&... __args)
    noexcept(noexcept(::new((void*)0) _Tp(std::declval<_Args>()...)))
    -> decltype(::new((void*)0) _Tp(std::declval<_Args>()...))
{
    return ::new((void*)__location) _Tp(std::forward<_Args>(__args)...);
}
#else
// >= C++20
template<typename T, typename... Args>
using construct_at = std::construct_at<T, Args...>;
#endif // C++20
} // ::details

#if __cplusplus < 202002L
#define constexpr_dtor
#define explicit_bool(v)
#else
#define constexpr_dtor constexpr
#define explicit_bool(v) explicit(v)
#endif

#ifndef AV_THROW_OR_ABORT
# if __cpp_exceptions
#  define AV_THROW_OR_ABORT(e) (throw (e))
# else
#  define AV_THROW_OR_ABORT(e) (std::abort())
# endif
#endif

template<typename _Er>
class bad_expected_access;

template<>
class bad_expected_access<void> : public std::exception
{
protected:
    bad_expected_access() noexcept { }
    bad_expected_access(const bad_expected_access&) = default;
    bad_expected_access(bad_expected_access&&) = default;
    bad_expected_access& operator=(const bad_expected_access&) = default;
    bad_expected_access& operator=(bad_expected_access&&) = default;
    ~bad_expected_access() = default;

public:

    [[nodiscard]]
    const char*
    what() const noexcept override
    { return "bad access to av::expected without expected value"; }
};

template<typename _Er>
class bad_expected_access : public bad_expected_access<void> {
public:
    explicit
        bad_expected_access(_Er __e) : _M_unex(std::move(__e)) { }

    // XXX const char* what() const noexcept override;

    [[nodiscard]]
    _Er&
    error() & noexcept
    { return _M_unex; }

    [[nodiscard]]
    const _Er&
    error() const & noexcept
    { return _M_unex; }

    [[nodiscard]]
    _Er&&
    error() && noexcept
    { return std::move(_M_unex); }

    [[nodiscard]]
    const _Er&&
    error() const && noexcept
    { return std::move(_M_unex); }

private:
    _Er _M_unex;
};

namespace __expected
{

template<typename _Tp>
struct _Guard
{
    static_assert( std::is_nothrow_move_constructible_v<_Tp> );

    constexpr explicit _Guard(_Tp& __x)
        : _M_guarded(std::addressof(__x)), _M_tmp(std::move(__x)) // nothrow
    { std::destroy_at(_M_guarded); }

    constexpr_dtor ~_Guard()
    {
        if (_M_guarded) [[unlikely]]
            details::construct_at(_M_guarded, std::move(_M_tmp));
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
        details::construct_at(__newval, std::forward<_Vp>(__arg));
    }
    else if constexpr (std::is_nothrow_move_constructible_v<_Tp>)
    {
        _Tp __tmp(std::forward<_Vp>(__arg)); // might throw
        std::destroy_at(__oldval);
        details::construct_at(__newval, std::move(__tmp));
    }
    else
    {
        _Guard<_Up> __guard(*__oldval);
        details::construct_at(__newval, std::forward<_Vp>(__arg)); // might throw
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
            details::construct_at(std::addressof(m_value), other.m_value);
        else
            details::construct_at(std::addressof(m_error), other.m_error);
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
            details::construct_at(std::addressof(m_value), std::move(other).m_value);
        else
            details::construct_at(std::addressof(m_error), std::move(other).m_error);
    }
#endif

    template<typename _Up = T>
    constexpr explicit_bool((!std::is_convertible_v<_Up, T>))
        expected(_Up&& __v)
        noexcept(std::is_nothrow_constructible_v<T, _Up>)
        : m_value(std::forward<_Up>(__v)), m_has_value(true)
    { }

    template<typename _Gr = E>
    constexpr explicit_bool((!std::is_convertible_v<const _Gr&, E>))
        expected(const unexpected<_Gr>& __u)
        noexcept(std::is_nothrow_constructible_v<E, const _Gr&>)
        : m_error(__u.error()), m_has_value(false)
    {}

    template<typename _Gr = E>
    constexpr explicit_bool((!std::is_convertible_v<_Gr, E>))
        expected(unexpected<_Gr>&& __u)
        noexcept(std::is_nothrow_constructible_v<E, _Gr>)
        : m_error(std::move(__u).error()), m_has_value(false)
    {}

#if 1
    constexpr_dtor ~expected() = default;
#else
    constexpr_dtor ~expected()
    {
        if (m_has_value)
            std::destroy_at(std::addressof(m_value));
        else
            std::destroy_at(std::addressof(m_error));
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
            this->assing_value(__x._M_val);
        else
            this->assign_error(__x._M_unex);
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
            assing_value(std::move(__x.m_value));
        else
            assign_error(std::move(__x.m_error));
        return *this;
    }

    template<typename _Up = T>
    constexpr expected&
    operator=(_Up&& __v)
    {
        assing_value(std::forward<_Up>(__v));
        return *this;
    }

    template<typename _Gr>
    constexpr expected&
    operator=(const unexpected<_Gr>& __e)
    {
        assign_error(__e.error());
        return *this;
    }

    template<typename _Gr>
    constexpr expected&
    operator=(unexpected<_Gr>&& __e)
    {
        assign_error(std::move(__e).error());
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
        assert(m_has_value);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr T*
    operator->() noexcept
    {
        assert(m_has_value);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr const T&
    operator*() const & noexcept
    {
        assert(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr T&
    operator*() & noexcept
    {
        assert(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr const T&&
    operator*() const && noexcept
    {
        assert(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr T&&
    operator*() && noexcept
    {
        assert(m_has_value);
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
        AV_THROW_OR_ABORT(bad_expected_access<E>(m_error));
    }

    constexpr T&
    value() &
    {
        if (m_has_value) [[likely]]
            return m_value;
        const auto& __unex = m_error;
        AV_THROW_OR_ABORT(bad_expected_access<E>(__unex));
    }

    constexpr const T&&
    value() const &&
    {
        if (m_has_value) [[likely]]
            return std::move(m_value);
        AV_THROW_OR_ABORT(bad_expected_access<E>(std::move(m_error)));
    }

    constexpr T&&
    value() &&
    {
        if (m_has_value) [[likely]]
            return std::move(m_value);
        AV_THROW_OR_ABORT(bad_expected_access<E>(std::move(m_error)));
    }

    constexpr const E&
    error() const & noexcept
    {
        assert(!m_has_value);
        return m_error;
    }

    constexpr E&
    error() & noexcept
    {
        assert(!m_has_value);
        return m_error;
    }

    constexpr const E&&
    error() const && noexcept
    {
        assert(!m_has_value);
        return std::move(m_error);
    }

    constexpr E&&
    error() && noexcept
    {
        assert(!m_has_value);
        return std::move(m_error);
    }


private:
    template<typename _Vp>
    constexpr void
    assing_value(_Vp&& __v)
    {
        if (m_has_value)
            m_value = std::forward<_Vp>(__v);
        else
        {
            __expected::__reinit(std::addressof(m_value),
                                 std::addressof(m_error),
                                 std::forward<_Vp>(__v));
            m_has_value = true;
        }
    }

    template<typename _Vp>
    constexpr void
    assign_error(_Vp&& __v)
    {
        if (m_has_value)
        {
            __expected::__reinit(std::addressof(m_error),
                                 std::addressof(m_value),
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
            details::construct_at(std::addressof(__rhs.m_value),
                                  std::move(m_value)); // might throw
            __rhs.m_has_value = true;
            std::destroy_at(std::addressof(m_value));
            details::construct_at(std::addressof(m_error),
                                  __guard.release());
            m_has_value = false;
        }
        else
        {
            __expected::_Guard<T> __guard(m_value);
            details::construct_at(std::addressof(m_error),
                                  std::move(__rhs.m_error)); // might throw
            m_has_value = false;
            std::destroy_at(std::addressof(__rhs.m_error));
            details::construct_at(std::addressof(__rhs.m_value),
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

} // ::av
