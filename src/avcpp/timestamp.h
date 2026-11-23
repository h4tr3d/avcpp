#pragma once

/**
 * Frame timestamps and their arithmetic.
 *
 * @file
 * @ingroup core
 */

#include <chrono>

#include "avutils.h"
#include "rational.h"

namespace av {

/**
 * @brief The Timestamp class represents timestamp value and it timebase
 *
 * Timestamp class can be treated as Fixed Point time representation where timestamp itself is a value and Time Base
 * is a multiplicator. Be careful with construct Timestamp from the std::chrono::duration with floating point @a rep
 *
 */
class Timestamp
{
public:
    Timestamp() noexcept;
    /**
     * Create a timestamp from a value and its time base.
     *
     * @param timestamp the value in `timebase` units
     * @param timebase  the time base that gives the value its meaning
     */
    Timestamp(int64_t timestamp, const Rational &timebase) noexcept;

    /**
     * @brief Create AvCpp/FFmpeg compatible timestamp value from the std::chrono::duration/boost::chrono::duration
     *
     * Duration class must not be a floating point to avoid lost precision: Timestamp in the FFmpeg internally are
     * int64_t with integer AVRAtional as timebase that also uses int64_t internally to store numerator and denominator.
     *
     * @param duration the duration to convert
     */
    template<typename Duration,
              typename = typename Duration::period,
              typename = std::enable_if_t<std::is_integral_v<typename Duration::rep>>>
    constexpr Timestamp(const Duration& duration)
    {
        using Ratio = typename Duration::period;

        static_assert(Ratio::num <= INT_MAX, "To prevent precision lost, ratio numerator must be less then INT_MAX");
        static_assert(Ratio::den <= INT_MAX, "To prevent precision lost, ratio denominator must be less then INT_MAX");

        m_timestamp = duration.count();
        m_timebase  = Rational(static_cast<int>(Ratio::num),
                               static_cast<int>(Ratio::den));
    }

    /**
     * @brief Create AvCpp/FFmpeg compatible timestamp value from the floating point
     * std::chrono::duration/boost::chrono::duration with given precision.
     *
     * PrecisionPeriod defines holded TimeBase
     *
     * @param duration the duration to convert
     */
    template<typename Duration,
              typename PrecisionPeriod,
              typename = typename Duration::period,
              typename = std::enable_if_t<std::is_floating_point_v<typename Duration::rep>>>
    constexpr Timestamp(const Duration& duration, PrecisionPeriod)
        : Timestamp(duration, Rational{static_cast<int>(PrecisionPeriod::num), static_cast<int>(PrecisionPeriod::den)})
    {
        using Ratio = typename Duration::period;
        static_assert(Ratio::num <= INT_MAX, "To prevent precision lost, ratio numerator must be less then INT_MAX");
        static_assert(Ratio::den <= INT_MAX, "To prevent precision lost, ratio denominator must be less then INT_MAX");
        static_assert(PrecisionPeriod::num <= INT_MAX, "To prevent precision lost, ratio numerator must be less then INT_MAX");
        static_assert(PrecisionPeriod::den <= INT_MAX, "To prevent precision lost, ratio denominator must be less then INT_MAX");
    }

    /**
     * @brief Create AvCpp/FFmpeg compatible timestamp value from the floating point
     * std::chrono::duration/boost::chrono::duration with given precision.
     *
     * @param duration the duration to convert
     * @param timebase the time base to store with the timestamp
     */
    template<typename Duration,
              typename = typename Duration::period,
              typename = std::enable_if_t<std::is_floating_point_v<typename Duration::rep>>>
    constexpr Timestamp(const Duration& duration, const Rational& timebase)
        : m_timebase(timebase)
    {
        using ValueType = typename Duration::rep;
        using Ratio = typename Duration::period;

        static_assert(Ratio::num <= INT_MAX, "To prevent precision lost, ratio numerator must be less then INT_MAX");
        static_assert(Ratio::den <= INT_MAX, "To prevent precision lost, ratio denominator must be less then INT_MAX");

        // rescale input ticks into integer one
        // m_timestamp = ts * Raio / m_timebase
        ValueType const b = static_cast<ValueType>(Ratio::num) * m_timebase.getDenominator();
        ValueType const c = static_cast<ValueType>(Ratio::den) * m_timebase.getNumerator();

        m_timestamp = static_cast<int64_t>(duration.count() * b / c);
    }


    /**
     * The raw timestamp value.
     *
     * @return the value in timebase() units, or NoPts when unset
     */
    int64_t         timestamp() const noexcept;
    /**
     * The timestamp converted to another time base.
     *
     * @param timebase the time base to convert to
     *
     * @return the value in `timebase` units
     */
    int64_t         timestamp(const Rational &timebase) const noexcept;
    /**
     * The time base of the timestamp.
     *
     * @return timestamp units per second
     */
    const Rational& timebase() const noexcept;

    /**
     * Whether the timestamp carries a value.
     *
     * @return `true` when it is not NoPts
     */
    operator bool() const noexcept;
    /**
     * Whether the timestamp is usable.
     *
     * @return `true` when the value is set and the time base is valid
     */
    bool isValid() const noexcept;
    /**
     * Whether the timestamp means "no timestamp".
     *
     * @return `true` when the value is NoPts
     */
    bool isNoPts() const noexcept;

    /**
     * The timestamp in seconds.
     *
     * @return the time in seconds as a floating point value
     */
    operator double() const noexcept;
    /**
     * The timestamp in seconds.
     *
     * @return the time in seconds as a floating point value
     */
    double seconds() const noexcept;

    /**
     * @brief Convert to the std::chrono::duration compatible value
     *
     * It possible to convert to the floating point duration without additional casts.
     *
     */
    template<typename Duration,
              typename = typename Duration::period,
              typename = typename Duration::rep>
    /**
     * Convert to a std::chrono duration.
     *
     * @tparam Duration the duration type to produce
     *
     * @return the timestamp as that duration; integral durations are rescaled,
     *         floating point ones are computed directly
     */
#if AVCPP_CXX_STANDARD >= 23
    constexpr Duration toDuration() const
#else
    Duration toDuration() const
#endif
    {
        using ValueType = typename Duration::rep;
        using Ratio = typename Duration::period;

        static_assert(Ratio::num <= INT_MAX, "To prevent precision lost, ratio numerator must be less then INT_MAX");
        static_assert(Ratio::den <= INT_MAX, "To prevent precision lost, ratio denominator must be less then INT_MAX");

        if constexpr (std::is_integral_v<ValueType>) {
            Rational dstTimebase(static_cast<int>(Ratio::num),
                                 static_cast<int>(Ratio::den));
            auto ts = m_timebase.rescale(m_timestamp, dstTimebase);
            return Duration(ts);
        } else {
            namespace dt = std::chrono;
            // ts = m_timestamp * m_timebase / dstTimebase
            ValueType const b = m_timebase.getNumerator()   * static_cast<ValueType>(Ratio::den);
            ValueType const c = m_timebase.getDenominator() * static_cast<ValueType>(Ratio::num);
            ValueType const ts = static_cast<ValueType>(m_timestamp) * b / c;
            return Duration{ts};
        }
    }

    /**
     * Add another timestamp, aligning the time bases.
     *
     * @param other the timestamp to add
     *
     * @return this timestamp
     */
    Timestamp& operator+=(const Timestamp &other);
    /**
     * Subtract another timestamp, aligning the time bases.
     *
     * @param other the timestamp to subtract
     *
     * @return this timestamp
     */
    Timestamp& operator-=(const Timestamp &other);
    /**
     * Multiply by another timestamp.
     *
     * @param other the factor
     *
     * @return this timestamp
     */
    Timestamp& operator*=(const Timestamp &other);
    /**
     * Divide by another timestamp.
     *
     * @param other the divisor
     *
     * @return this timestamp
     */
    Timestamp& operator/=(const Timestamp &other);

private:
    int64_t  m_timestamp = av::NoPts;
    Rational m_timebase = av::TimeBaseQ;
};


//
// Comparation
//

inline
/**
 * Compare two timestamps, taking their time bases into account.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when the left one is later
 */
bool operator>(const Timestamp& left, const Timestamp &right) noexcept
{
    return av_compare_ts(left.timestamp(), left.timebase(),
                         right.timestamp(), right.timebase()) > 0;
}


inline
/**
 * Compare two timestamps, taking their time bases into account.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when the left one is earlier
 */
bool operator<(const Timestamp& left, const Timestamp &right) noexcept
{
    return av_compare_ts(left.timestamp(), left.timebase(),
                         right.timestamp(), right.timebase()) < 0;
}


inline
/**
 * Compare two timestamps, taking their time bases into account.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when both denote the same moment
 */
bool operator==(const Timestamp& left, const Timestamp &right) noexcept
{
    return av_compare_ts(left.timestamp(), left.timebase(),
                         right.timestamp(), right.timebase()) == 0;
}

inline
/**
 * Compare two timestamps.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when the moments differ
 */
bool operator!=(const Timestamp& left, const Timestamp &right) noexcept
{
    return !operator==(left, right);
}

inline
/**
 * Compare two timestamps.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when the left one is not earlier
 */
bool operator>=(const Timestamp& left, const Timestamp& right) noexcept
{
    return av_compare_ts(left.timestamp(), left.timebase(),
                         right.timestamp(), right.timebase()) >= 0;
}

inline
/**
 * Compare two timestamps.
 *
 * @param left  the left timestamp
 * @param right the right timestamp
 *
 * @return `true` when the left one is not later
 */
bool operator<=(const Timestamp& left, const Timestamp& right) noexcept
{
    return av_compare_ts(left.timestamp(), left.timebase(),
                         right.timestamp(), right.timebase()) <= 0;
}

//
// Math operations
//

/**
 * Add two timestamps, aligning the time bases.
 *
 * @param left  the left operand
 * @param right the right operand
 *
 * @return the sum
 */
Timestamp operator+(const Timestamp& left, const Timestamp &right) noexcept;
/**
 * Subtract two timestamps, aligning the time bases.
 *
 * @param left  the left operand
 * @param right the right operand
 *
 * @return the difference
 */
Timestamp operator-(const Timestamp& left, const Timestamp &right) noexcept;
/**
 * Multiply two timestamps.
 *
 * @param left  the left operand
 * @param right the right operand
 *
 * @return the product
 */
Timestamp operator*(const Timestamp& left, const Timestamp &right) noexcept;
/**
 * Divide two timestamps.
 *
 * @param left  the left operand
 * @param right the right operand
 *
 * @return the quotient
 */
Timestamp operator/(const Timestamp& left, const Timestamp &right) noexcept;

//
// Output
//
inline
/**
 * Print the timestamp as "value*timebase", or NO_PTS.
 *
 * @param ost the stream to write to
 * @param ts  the timestamp to print
 *
 * @return the stream, for chaining
 */
std::ostream& operator<<(std::ostream &ost, const Timestamp &ts)
{
    if (ts.isNoPts()) {
        ost << "NO_PTS";
    } else {
        ost << ts.timestamp() << '*' << ts.timebase();
    }
    return ost;
}

} // ::av

#ifdef __cpp_lib_format
#include <format>

// std::format
template <typename CharT>
struct std::formatter<av::Timestamp, CharT>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename ParseContext>
    auto format(const av::Timestamp& ts, ParseContext& ctx) const
    {
        if (ts.isNoPts()) {
            return std::format_to(ctx.out(), "NO_PTS");
        }
        return std::format_to(ctx.out(), "{}*{}", ts.timestamp(), ts.timebase());
    }
};
#endif
