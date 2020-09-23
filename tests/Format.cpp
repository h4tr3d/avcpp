#include <catch2/catch.hpp>

#include <vector>

#include "format.h"
#include "codec.h"

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
}
