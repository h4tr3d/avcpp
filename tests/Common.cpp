#include <algorithm>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include "avcpp/avconfig.h"
#include "avcpp/avutils.h"

#include <random>
#include <vector>

#ifdef __cpp_lib_print
#    include <format>
#endif

#include "avcpp/rect.h"

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif

using namespace std;

TEST_CASE("Common tools and helpers", "[Common]")
{
#ifdef __cpp_lib_print
    SECTION("std::format formatter for Rect")
    {
        av::Rect rect {50, 50, 640, 480};
        auto str = std::format("{}", rect);
        CHECK(str == "640x480+50+50");

        CHECK(std::format("{}", av::Rect(-50, -50, 640, 480)) == "640x480-50-50");
    }
#endif

#if AVCPP_CXX_STANDARD >= 20
    SECTION("guessValue and ArrayView")
    {
        int ratesRaw[] = {
            8'000,
            11'025,
            16000,
            22050,
            44'100,
            48'000,
            88'200,
            96'000,
            176'400,
            192'000,
            352'800,
            384'000,
            0 // end marker
        };

        std::random_device rd;
        std::mt19937 gen {rd()};

        auto ratesSized = av::make_array_view_size<int>(ratesRaw, std::size(ratesRaw) - 1); // exclude last zero
        auto ratesUntil = av::make_array_view_until<int>(ratesRaw, 0);

        REQUIRE(ratesSized.back() == 384'000);

        std::ranges::shuffle(ratesUntil, gen);
        for (auto&& r : ratesRaw) {
            cout << "rate: " << r << endl;
        }

        {
            int rate;
            rate = av::guessValue(9'000, ratesSized);
            REQUIRE(rate == 8'000);

            rate = av::guessValue(10'000, ratesUntil);
            REQUIRE(rate == 11'025);

            rate = av::guessValue(0, ratesUntil);
            REQUIRE(rate == 8'000);

            rate = av::guessValue(1'000'000, ratesUntil);
            REQUIRE(rate == 384'000);
        }

        // Legacy
        {
            int rate;
            rate = av::guessValue(9'000, ratesRaw, av::EqualComparator<int>(0));
            REQUIRE(rate == 8'000);

            rate = av::guessValue(10'000, ratesRaw, av::EqualComparator<int>(0));
            REQUIRE(rate == 11'025);

            rate = av::guessValue(0, ratesRaw, av::EqualComparator<int>(0));
            REQUIRE(rate == 8'000);

            rate = av::guessValue(1'000'000, ratesRaw, av::EqualComparator<int>(0));
            REQUIRE(rate == 384'000);
        }
    }
#endif

}
