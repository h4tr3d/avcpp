#include <catch2/catch_test_macros.hpp>

#include <vector>

#ifdef __cpp_lib_print
#    include <format>
#endif

#include "avcpp/codec.h"
#include "avcpp/ffmpeg.h"

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

TEST_CASE("Codec: subtitle codecs report their direction", "[Codec]")
{
    // Before FFmpeg 5.1 subtitle encoders provide only the legacy encode_sub
    // callback, so it must count as encoding support.
    auto encoder = av::findEncodingCodec("subrip");
    REQUIRE_FALSE(encoder.isNull());
    CHECK(encoder.canEncode());
    CHECK_FALSE(encoder.canDecode());

    auto decoder = av::findDecodingCodec("subrip");
    REQUIRE_FALSE(decoder.isNull());
    CHECK(decoder.canDecode());
    CHECK_FALSE(decoder.canEncode());
}
