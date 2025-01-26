#include <catch2/catch.hpp>

#include <vector>

#ifdef __cpp_lib_print
#  include <format>
#endif

#include "format.h"
#include "codec.h"
#include "ffmpeg.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif

TEST_CASE("Format Core functionality", "[Format]")
{
    SECTION("guessEncodingCodec()") {
        auto out_format = av::guessOutputFormat("matroska");
        CHECK(out_format.raw());

        auto codec = av::guessEncodingCodec(out_format, "matroska", "out.mkv", nullptr, AVMEDIA_TYPE_VIDEO);
        CHECK(codec.raw());
    }

    SECTION("setFormat") {
        auto tmp_format = av::guessOutputFormat("matroska");
        CHECK(tmp_format.raw());

        av::OutputFormat of;
        of.setFormat(tmp_format.raw());

        CHECK(tmp_format.raw() == of.raw());
    }

#ifdef __cpp_lib_print
    SECTION("std::format formatter")
    {
        {
            auto const format = av::guessOutputFormat("matroska");
            auto str = std::format("{}", format);
            CHECK(str == "matroska");

            str = std::format("{:l}", format);
            CHECK(str == "Matroska");
        }

        {
            av::InputFormat format {"matroska"};
            auto str = std::format("{}", format);
            CHECK(str == "matroska,webm");

            str = std::format("{:l}", format);
            CHECK(str == "Matroska / WebM");
        }
    }
#endif

}
