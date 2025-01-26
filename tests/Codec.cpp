#include <catch2/catch.hpp>

#include <vector>

#ifdef __cpp_lib_print
#    include <format>
#endif

#include "codec.h"
#include "ffmpeg.h"

#ifdef _MSC_VER
#    pragma warning(disable : 4702) // Disable warning: unreachable code
#endif

TEST_CASE("Codec", "[Codec]")
{
#ifdef __cpp_lib_print
    SECTION("std::format formatter")
    {
        av::Codec codec = av::findDecodingCodec("h264");
        auto str = std::format("{}", codec);
        CHECK(str == "h264");

        str = std::format("{:l}", codec);
        CHECK(str == "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10");
    }
#endif
}
