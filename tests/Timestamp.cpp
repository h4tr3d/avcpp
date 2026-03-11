#include <version>

#ifdef __cpp_lib_print
#  include <format>
#endif

#include "avcpp/rational.h"
#include "avcpp/timestamp.h"

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif


#if AVCPP_CXX_STANDARD < 20 ||  __cpp_lib_chrono < 201907L
#include <sstream>
#include <iostream>
#include <type_traits>
#include <chrono>

template<typename Rep, typename Period>
std::ostream& operator<<(std::ostream& ost, const std::chrono::duration<Rep, Period>& dur)
{
    using namespace std::literals;

    std::ostringstream s;
    s.flags(ost.flags());
    s.imbue(ost.getloc());
    s.precision(ost.precision());

    s << +dur.count();

    // Ref: https://en.cppreference.com/w/cpp/chrono/duration/operator_ltlt.html
    if      constexpr (std::is_same_v<Period, std::atto>) s << "as"sv;
    else if constexpr (std::is_same_v<Period, std::femto>) s << "fs"sv;
    else if constexpr (std::is_same_v<Period, std::pico>) s << "fs"sv;
    else if constexpr (std::is_same_v<Period, std::nano>) s << "ns"sv;
    else if constexpr (std::is_same_v<Period, std::micro>) s << "us"sv;
    else if constexpr (std::is_same_v<Period, std::milli>) s << "ms"sv;
    else if constexpr (std::is_same_v<Period, std::centi>) s << "cs"sv;
    else if constexpr (std::is_same_v<Period, std::deci>) s << "ds"sv;
    else if constexpr (std::is_same_v<Period, std::ratio<1>>) s << "s"sv;
    else if constexpr (std::is_same_v<Period, std::deca>) s << "das"sv;
    else if constexpr (std::is_same_v<Period, std::hecto>) s << "hs"sv;
    else if constexpr (std::is_same_v<Period, std::kilo>) s << "ks"sv;
    else if constexpr (std::is_same_v<Period, std::mega>) s << "Ms"sv;
    else if constexpr (std::is_same_v<Period, std::giga>) s << "Gs"sv;
    else if constexpr (std::is_same_v<Period, std::tera>) s << "Ts"sv;
    else if constexpr (std::is_same_v<Period, std::peta>) s << "Ps"sv;
    else if constexpr (std::is_same_v<Period, std::exa>) s << "Es"sv;
    else if constexpr (std::is_same_v<Period, std::ratio<60>>) s << "min"sv;
    else if constexpr (std::is_same_v<Period, std::ratio<3600>>) s << "h"sv;
    else if constexpr (std::is_same_v<Period, std::ratio<86400>>) s << "d"sv;
    else if constexpr (Period::den == 1) s << '[' << Period::num << "]s"sv;
    else
        s << '[' << Period::num << '/' << Period::num << "]s"sv;
    ost << std::move(s).str();
    return ost;
}
#endif

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Core::Timestamp", "Timestamp")
{
    SECTION("Overflow operator+(a,b)")
    {
        {
            auto const tsLimit = std::numeric_limits<int64_t>::max() / 48000;

            av::Timestamp t {
                tsLimit, av::Rational {1, 48000}
            };
            av::Timestamp inc {48000, t.timebase()}; // 1s

            auto v1 = t;
            v1 += inc;

            auto v2 = t + inc;
            auto v3 = inc + t;

            CHECK(v1 == v2);
            CHECK(v1 == v3);
        }
    }

    SECTION("Floating point std::duration")
    {
        {
            std::chrono::duration<double> seconds{1.53f};

            av::Timestamp fromDuration{std::chrono::duration_cast<std::chrono::milliseconds>(seconds)};

            INFO("std::chrono::duration<double>: " << seconds.count());
            INFO("av::Timestamp::seconds:        " << fromDuration.seconds());
            INFO("av::Timestamp:                 " << fromDuration);
            REQUIRE(std::abs(fromDuration.seconds() - seconds.count()) <= 0.001);

            auto toDoubleDuration = fromDuration.toDuration<std::chrono::duration<double>>();
            INFO("toDoubleDuration: " << toDoubleDuration);
            REQUIRE(std::abs(toDoubleDuration.count() - seconds.count()) <= 0.001);
        }

        // Double and Ratio different to {1,1}
        {
            using Ratio = std::milli;

            std::chrono::duration<double, Ratio> ms{1530.17};
            av::Timestamp fromDuration{std::chrono::duration_cast<std::chrono::microseconds>(ms)};
            INFO("fromDuration(ms): " << fromDuration.seconds()
                                      << "s, val: " << fromDuration);
            REQUIRE(std::abs(fromDuration.seconds() - ms.count() * Ratio::num / Ratio::den) <= 0.00001);

            auto toDoubleDuration = fromDuration.toDuration<std::chrono::duration<double, Ratio>>();
            INFO("toDoubleDuration: " << toDoubleDuration);
            REQUIRE(std::abs(toDoubleDuration.count() - ms.count()) <= 0.00001);
        }

        // Double and non-standard ratio
        {
            using Ratio = std::ratio<1, 48000>;

            std::chrono::duration<double, Ratio> dur{1530.17};
            av::Timestamp fromDuration{std::chrono::duration_cast<std::chrono::nanoseconds>(dur)};
            INFO("fromDuration(dur): " << fromDuration.seconds()
                                       << "s, val: " << fromDuration);
            REQUIRE(std::abs(fromDuration.seconds() - dur.count() * Ratio::num / Ratio::den) <= 0.00001);

            auto toDoubleDuration = fromDuration.toDuration<std::chrono::duration<double, Ratio>>();
            INFO("toDoubleDuration: " << toDoubleDuration);
            REQUIRE(std::abs(toDoubleDuration.count() - dur.count()) <= 0.0001);
        }

        // Ctor from the floating point duration
        {
            std::chrono::duration<double> seconds{1.57f};

            {
                av::Timestamp fromDuration{seconds, std::milli{}};

                auto const precision = fromDuration.timebase().getDouble();

                INFO("std::chrono::duration<double>:count: " << seconds.count());
                INFO("std::chrono::duration<double>:       " << seconds);
                INFO("av::Timestamp::seconds:              " << fromDuration.seconds());
                INFO("av::Timestamp:                       " << fromDuration);
                REQUIRE(std::abs(fromDuration.seconds() - seconds.count()) < precision);

                auto toDoubleDuration = fromDuration.toDuration<std::chrono::duration<double>>();
                INFO("toDoubleDuration: " << toDoubleDuration);
                REQUIRE(std::abs(toDoubleDuration.count() - seconds.count()) < precision);
            }

            {
                av::Timestamp fromDuration{seconds, av::Rational{1, 1000}};

                auto const precision = fromDuration.timebase().getDouble();

                INFO("std::chrono::duration<double>:count: " << seconds.count());
                INFO("std::chrono::duration<double>:       " << seconds);
                INFO("av::Timestamp::seconds:              " << fromDuration.seconds());
                INFO("av::Timestamp:                       " << fromDuration);
                REQUIRE(std::abs(fromDuration.seconds() - seconds.count()) < precision);

                auto toDoubleDuration = fromDuration.toDuration<std::chrono::duration<double>>();
                INFO("toDoubleDuration: " << toDoubleDuration);
                REQUIRE(std::abs(toDoubleDuration.count() - seconds.count()) < precision);
            }
        }
    }


#ifdef __cpp_lib_print
    SECTION("std::format formatter")
    {
        av::Timestamp ts(48000, av::Rational {1, 48000}); // 1s
        auto str = std::format("{}", ts);
        CHECK(str == "48000*1/48000");
    }
#endif

}
