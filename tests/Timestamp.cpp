#include <catch2/catch.hpp>

#include <vector>

#ifdef __cpp_lib_print
#  include <format>
#endif

#include "rational.h"
#include "timestamp.h"

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

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

#if 0
        av::Timestamp t1(48000, av::Rational {1, 48000}); // 1s
        av::Timestamp t2 = t1;
        av::Timestamp t3 = t1;
        for (int64_t i = 0; i < 4194258; ++i) {
            t1 = t1 + av::Timestamp {1024, t1.timebase()}; // fail case
            t2 += av::Timestamp {1024, t2.timebase()}; // good
            t3 = av::Timestamp {1024, t1.timebase()} + t3;

            CHECK(t3 == t2);
            CHECK(t1 == t2);
            CHECK(t1.seconds() >= 1.0);

            if (t1 != t2 || t2 != t3 || t1.seconds() < 1.0) {
                CHECK(i < 0); // always fail, just for report
                break;
            }
        }
#endif
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
