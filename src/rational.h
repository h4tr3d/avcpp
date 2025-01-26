#pragma once

#include <utility>
#include <iostream>
#include <memory>
#include <type_traits>

#include "ffmpeg.h"
#include "avutils.h"

#ifdef AVCPP_USE_SPACESHIP_OPERATOR
#  include <compare>
#endif

namespace av
{

#define AV_TIME_BASE_Q_CPP          AVRational{1, AV_TIME_BASE}

enum
{
    RationalMaxPrecision = 5
};

class Rational
{
public:
    Rational() noexcept;
    Rational(int numerator, int denominator = 1) noexcept;
    Rational(const AVRational &value) noexcept;
    Rational(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    AVRational& getValue() noexcept { return m_value; }
    const AVRational& getValue() const noexcept { return m_value; }
    void setValue(const AVRational &newValue) noexcept { m_value = newValue; }

    void setNumerator(int numerator) noexcept { m_value.num = numerator; }
    void setDenominator(int denominator) noexcept { m_value.den = denominator; }

    int    getNumerator() const noexcept{ return m_value.num; }
    int    getDenominator() const noexcept { return m_value.den; }
    double getDouble() const noexcept { return (double)m_value.num / (double)m_value.den; }

    static Rational fromDouble(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    int64_t rescale(int64_t srcValue, const Rational &dstBase) const noexcept;

    void dump() const noexcept;

    Rational& operator=  (const AVRational &value) noexcept;
    Rational& operator=  (double value) noexcept;

    bool      operator== (const Rational   &other) const noexcept;
#ifdef AVCPP_USE_SPACESHIP_OPERATOR
    std::strong_ordering operator<=>(const Rational &other) const noexcept
    {
        switch (threewaycmp(other)) {
        case -1:
            return std::strong_ordering::less;
        case 0:
            return std::strong_ordering::equal;
        case 1:
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal; // make a compiler happy
    }
#else
    bool operator!= (const Rational &other) const noexcept {
        return threewaycmp(other) != 0;
    }
    bool operator<(const Rational &other) const noexcept {
        return threewaycmp(other) < 0;
    }
    bool operator>(const Rational &other) const noexcept {
        return threewaycmp(other) > 0;
    }
    bool operator<=(const Rational &other) const noexcept {
        return (*this < other) || (*this == other);
    }
    bool operator>=(const Rational &other) const noexcept {
        return (*this > other) || (*this == other);
    }
#endif

    Rational  operator+  (const Rational   &value) const noexcept;
    Rational  operator-  (const Rational   &value) const noexcept;
    Rational  operator*  (const Rational   &value) const noexcept;
    Rational  operator/  (const Rational   &value) const noexcept;

    double operator() () const noexcept;

    operator const AVRational&() const noexcept
    {
        return m_value;
    }

private:
    int threewaycmp(const Rational &other) const noexcept;

private:
    AVRational m_value;
};


template<typename T>
auto operator/ (T num, Rational value) ->
    std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>, Rational>
{
    return Rational{num, 1} / value;
}

template<typename T>
auto operator/ (Rational value, T num) ->
    std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>, Rational>
{
    return value / Rational{num, 1};
}

inline std::ostream& operator<< (std::ostream &stream, const Rational &value)
{
    stream << value.getNumerator() << '/' << value.getDenominator();
    return stream;

}

inline std::istream& operator>> (std::istream &stream, Rational &value)
{
    char       ch;
    AVRational temp;

    try
    {
        stream >> temp.num >> ch >> temp.den;
        if (ch == '/')
        {
            value.setNumerator(temp.num);
            value.setDenominator(temp.den);
        }
    }
    catch (const std::exception &e)
    {}

    return stream;
}


} // ::av


#ifdef __cpp_lib_format
#include <format>
// std::format
template <typename CharT>
struct std::formatter<av::Rational, CharT>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template<typename ParseContext>
    auto format(const av::Rational& value, ParseContext& ctx) const
    {
        return std::format_to(ctx.out(), "{}/{}", value.getNumerator(), value.getDenominator());
    }
};
#endif
