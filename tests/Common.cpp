#include <catch2/catch.hpp>

#include <vector>

#ifdef __cpp_lib_print
#    include <format>
#endif

#include <rect.h>

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif

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
}
