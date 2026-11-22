#pragma once

/**
 * Rational numbers, time bases and arithmetic on them.
 *
 * @file
 * @ingroup core
 */

#include <utility>
#include <iostream>
#include <memory>
#include <type_traits>

#include "ffmpeg.h"
#include "avutils.h"

#if AVCPP_USE_SPACESHIP_OPERATOR
#  include <compare>
#endif

namespace av
{

/**
 * The global FFmpeg time base as an AVRational constant.
 *
 * Used where an `AVRational` is required; av::TimeBaseQ is the avcpp wrapper.
 */
#define AV_TIME_BASE_Q_CPP          AVRational{1, AV_TIME_BASE}

enum
{
    RationalMaxPrecision = 5
};

/**
 * Rational number: a numerator and a denominator.
 *
 * Widths, heights and frame rates in FFmpeg are rationals, and so are the time
 * bases that give meaning to timestamps. The class keeps the FFmpeg
 * representation and adds comparison and arithmetic that account for different
 * denominators, so `1/25` and `2/50` compare equal.
 *
 * A denominator of zero is preserved rather than corrected: it is what FFmpeg
 * uses for "unknown", and isValid() reports it.
 */
class Rational
{
public:
    /**
     * Create zero, in the sense of 0/1.
     */
    Rational() noexcept;
    /**
     * Create a rational number.
     *
     * @param numerator   the numerator
     * @param denominator the denominator; zero means "unknown"
     */
    Rational(int numerator, int denominator = 1) noexcept;
    /**
     * Wrap an FFmpeg rational.
     *
     * @param value the value to copy
     */
    Rational(const AVRational &value) noexcept;
    /**
     * Approximate a floating point value as a rational.
     *
     * @param value        the number to approximate
     * @param maxPrecision maximum denominator to try, in decimal digits
     */
    Rational(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    /**
     * Mutable access to the underlying FFmpeg value.
     *
     * @return a reference to the stored rational
     */
    AVRational& getValue() noexcept { return m_value; }
    /**
     * The underlying FFmpeg value.
     *
     * @return a reference to the stored rational
     */
    const AVRational& getValue() const noexcept { return m_value; }
    /**
     * Replace the value.
     *
     * @param newValue the rational to store
     */
    void setValue(const AVRational &newValue) noexcept { m_value = newValue; }

    /**
     * Set the numerator.
     *
     * @param numerator the new numerator
     */
    void setNumerator(int numerator) noexcept { m_value.num = numerator; }
    /**
     * Set the denominator.
     *
     * @param denominator the new denominator; zero means "unknown"
     */
    void setDenominator(int denominator) noexcept { m_value.den = denominator; }

    /**
     * The numerator.
     *
     * @return the stored numerator
     */
    int    getNumerator() const noexcept{ return m_value.num; }
    /**
     * The denominator.
     *
     * @return the stored denominator
     */
    int    getDenominator() const noexcept { return m_value.den; }
    /**
     * The value as a floating point number.
     *
     * @return numerator divided by denominator; infinity for a zero denominator
     */
    double getDouble() const noexcept { return (double)m_value.num / (double)m_value.den; }

    /**
     * Approximate a floating point value as a rational.
     *
     * @param value        the number to approximate
     * @param maxPrecision maximum denominator to try, in decimal digits
     *
     * @return the closest rational within the precision limit
     */
    static Rational fromDouble(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    /**
     * Rescale a value from this base to another.
     *
     * @param srcValue the value in this time base
     * @param dstBase  the time base to convert to
     *
     * @return the value in `dstBase`, rounded the way FFmpeg rounds
     */
    int64_t rescale(int64_t srcValue, const Rational &dstBase) const noexcept;

    /**
     * Print the value through the FFmpeg log system.
     */
    void dump() const noexcept;

    /**
     * Assign an FFmpeg rational.
     *
     * @param value the value to store
     *
     * @return this rational
     */
    Rational& operator=  (const AVRational &value) noexcept;
    /**
     * Assign an approximation of a floating point value.
     *
     * @param value the number to approximate
     *
     * @return this rational
     */
    Rational& operator=  (double value) noexcept;

    /**
     * Compare two rationals by value, not by representation.
     *
     * @param other the rational to compare with
     *
     * @return `true` when the values are equal, so 1/25 equals 2/50
     */
    bool      operator== (const Rational   &other) const noexcept;
#if AVCPP_USE_SPACESHIP_OPERATOR
    /**
     * Order two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return the three-way comparison of the values
     */
    std::strong_ordering operator<=>(const Rational &other) const noexcept
    {
        if (auto const cmp = threewaycmp(other); cmp == 0) {
            return std::strong_ordering::equal;
        } else if (cmp < 0) {
            return std::strong_ordering::less;
        } else {
            return std::strong_ordering::greater;
        }
    }
#else
    /**
     * Compare two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return `true` when the values differ
     */
    bool operator!= (const Rational &other) const noexcept {
        return threewaycmp(other) != 0;
    }
    /**
     * Compare two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return `true` when this value is smaller
     */
    bool operator<(const Rational &other) const noexcept {
        return threewaycmp(other) < 0;
    }
    /**
     * Compare two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return `true` when this value is greater
     */
    bool operator>(const Rational &other) const noexcept {
        return threewaycmp(other) > 0;
    }
    /**
     * Compare two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return `true` when this value is smaller or equal
     */
    bool operator<=(const Rational &other) const noexcept {
        return (*this < other) || (*this == other);
    }
    /**
     * Compare two rationals by value.
     *
     * @param other the rational to compare with
     *
     * @return `true` when this value is greater or equal
     */
    bool operator>=(const Rational &other) const noexcept {
        return (*this > other) || (*this == other);
    }
#endif

    /**
     * Add another rational.
     *
     * @param value the value to add
     *
     * @return the sum, reduced to a common denominator
     */
    Rational  operator+  (const Rational   &value) const noexcept;
    /**
     * Subtract another rational.
     *
     * @param value the value to subtract
     *
     * @return the difference
     */
    Rational  operator-  (const Rational   &value) const noexcept;
    /**
     * Multiply by another rational.
     *
     * @param value the factor
     *
     * @return the product
     */
    Rational  operator*  (const Rational   &value) const noexcept;
    /**
     * Divide by another rational.
     *
     * @param value the divisor; a zero rational produces an invalid result
     *
     * @return the quotient
     */
    Rational  operator/  (const Rational   &value) const noexcept;

    /**
     * The value as a floating point number.
     *
     * @return numerator divided by denominator
     */
    double operator() () const noexcept;

    /**
     * Implicit conversion to the FFmpeg type.
     *
     * @return a reference to the stored value, so it can be passed to FFmpeg
     */
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
/**
 * Divide a number by a rational.
 *
 * @tparam T arithmetic type
 *
 * @param num   the dividend
 * @param value the divisor
 *
 * @return the quotient as a rational
 */
auto operator/ (T num, Rational value) ->
    std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>, Rational>
{
    return Rational{num, 1} / value;
}

template<typename T>
/**
 * Divide a rational by a number.
 *
 * @tparam T arithmetic type
 *
 * @param value the dividend
 * @param num   the divisor
 *
 * @return the quotient as a rational
 */
auto operator/ (Rational value, T num) ->
    std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>, Rational>
{
    return value / Rational{num, 1};
}

/**
 * Print the rational as "numerator/denominator".
 *
 * @param stream the stream to write to
 * @param value  the rational to print
 *
 * @return the stream, for chaining
 */
inline std::ostream& operator<< (std::ostream &stream, const Rational &value)
{
    stream << value.getNumerator() << '/' << value.getDenominator();
    return stream;

}

/**
 * Read a rational written as "numerator/denominator".
 *
 * @param stream the stream to read from
 * @param value  the rational to fill; left unchanged when parsing fails
 *
 * @return the stream, for chaining
 */
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
