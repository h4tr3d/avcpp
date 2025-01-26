#include <catch2/catch.hpp>

#include <vector>

#ifdef __cpp_lib_print
#    include <format>
#endif

#include "pixelformat.h"
#include "sampleformat.h"
#include "ffmpeg.h"

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif

TEST_CASE("PixelFormat / SampleFormat", "[PixelFormat][SampleFormat]")
{
#ifdef __cpp_lib_print
    SECTION("std::format formatter for PixelFormat")
    {
        {
            av::PixelFormat pixfmt {"yuv420p"};
            auto str = std::format("{}", pixfmt);
            CHECK(str == "yuv420p");
        }

        {
            av::PixelFormat pixfmt {AV_PIX_FMT_YUV420P};
            auto str = std::format("{}", pixfmt);
            CHECK(str == "yuv420p");
        }
    }

    SECTION("std::format formatter for SampleFormat")
    {
        {
            av::SampleFormat samplefmt {"s16"};
            auto str = std::format("{}", samplefmt);
            CHECK(str == "s16");
        }

        {
            av::SampleFormat samplefmt {AV_SAMPLE_FMT_S16};
            auto str = std::format("{}", samplefmt);
            CHECK(str == "s16");
        }
    }
#endif
}
