#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "avcpp/subtitle.h"
#include "avcpp/timestamp.h"
#include "avcpp/averror.h"
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/packet.h"

namespace {

constexpr int BITMAP_W = 16;
constexpr int BITMAP_H = 8;

std::vector<std::uint8_t> makeBitmap()
{
    std::vector<std::uint8_t> data(static_cast<std::size_t>(BITMAP_W) * BITMAP_H);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<std::uint8_t>(i % 4);
    return data;
}

const std::uint32_t palette[] = {
    0x00000000,
    0xFFFFFFFF,
    0xFF000000,
    0xFFFFFF00,
};

} // namespace

TEST_CASE("SubtitleRect: text properties", "[Subtitle][Rect]")
{
    av::SubtitleFrame frame;
    auto rect = frame.addRect(SUBTITLE_TEXT);

    REQUIRE(rect.isValid());
    CHECK(rect.type() == SUBTITLE_TEXT);

    rect.setArea(1, 2, 3, 4);
    CHECK(rect.x() == 1);
    CHECK(rect.y() == 2);
    CHECK(rect.width() == 3);
    CHECK(rect.height() == 4);

    rect.setText("hello");
    CHECK(rect.text() == "hello");
    CHECK(rect.ass().empty());

    rect.setAss("Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,hello");
    CHECK(rect.ass().find("hello") != std::string_view::npos);

    CHECK_FALSE(rect.isForced());
    rect.setForced(true);
    CHECK(rect.isForced());
    rect.setForced(false);
    CHECK_FALSE(rect.isForced());

    rect.clearText();
    CHECK(rect.text().empty());
    CHECK_FALSE(rect.ass().empty());

    rect.clearAss();
    CHECK(rect.ass().empty());
}

TEST_CASE("SubtitleRect: bitmap and palette", "[Subtitle][Rect][Bitmap]")
{
    auto bitmap = makeBitmap();

    av::SubtitleFrame frame;
    auto rect = frame.addRect();
    CHECK(rect.type() == SUBTITLE_NONE);

    rect.setBitmap(bitmap.data(), BITMAP_W, BITMAP_W, BITMAP_H);
    CHECK(rect.type() == SUBTITLE_BITMAP);
    CHECK(rect.width() == BITMAP_W);
    CHECK(rect.height() == BITMAP_H);
    CHECK(rect.linesize(0) == BITMAP_W);
    REQUIRE(rect.data(0) != nullptr);
    CHECK(std::memcmp(rect.data(0), bitmap.data(), bitmap.size()) == 0);

    rect.setPalette(palette, 4);
    CHECK(rect.nbColors() == 4);
    REQUIRE(rect.data(1) != nullptr);
    CHECK(std::memcmp(rect.data(1), palette, sizeof(palette)) == 0);

#if AVCPP_CXX_STANDARD >= 20
    auto bitmapSpan = rect.bitmap();
    REQUIRE(bitmapSpan.size() == bitmap.size());
    CHECK(std::memcmp(bitmapSpan.data(), bitmap.data(), bitmap.size()) == 0);

    auto paletteSpan = rect.palette();
    REQUIRE(paletteSpan.size() == 4);
    CHECK(std::memcmp(paletteSpan.data(), palette, sizeof(palette)) == 0);

    // The span overloads replace the existing data and copy it.
    std::vector<std::uint8_t> otherBitmap(bitmap.rbegin(), bitmap.rend());
    rect.setBitmap(otherBitmap, BITMAP_W, BITMAP_W, BITMAP_H);
    CHECK(std::memcmp(rect.data(0), otherBitmap.data(), otherBitmap.size()) == 0);

    std::vector<std::uint32_t> otherPalette{ 1, 2, 3, 4 };
    rect.setPalette(otherPalette);
    CHECK(rect.nbColors() == 4);
    CHECK(std::memcmp(rect.data(1), otherPalette.data(), otherPalette.size() * sizeof(std::uint32_t)) == 0);
#endif
}

TEST_CASE("SubtitleRect: invalid data is rejected", "[Subtitle][Rect]")
{
    av::SubtitleFrame frame;
    auto rect = frame.addRect(SUBTITLE_BITMAP);

    std::error_code ec;
    rect.setBitmap(nullptr, BITMAP_W, BITMAP_W, BITMAP_H, ec);
    CHECK(ec == av::Errors::InvalidArgument);

    ec.clear();
    rect.setBitmap(makeBitmap().data(), BITMAP_W, 0, BITMAP_H, ec);
    CHECK(ec == av::Errors::InvalidArgument);

    ec.clear();
    rect.setPalette(nullptr, 4, ec);
    CHECK(ec == av::Errors::InvalidArgument);

    CHECK_THROWS_AS(rect.setPalette(nullptr, 4), av::Exception);
}

TEST_CASE("SubtitleFrame: rectangle list", "[Subtitle][Frame]")
{
    av::SubtitleFrame frame;
    CHECK(frame.numRects() == 0);
    CHECK_FALSE(frame.isValid());

    auto first = frame.addRect(SUBTITLE_TEXT);
    first.setText("first");
    auto second = frame.addRect(SUBTITLE_ASS);
    second.setAss("Dialogue: second");

    CHECK(frame.numRects() == 2);
    CHECK(frame.isValid());
    CHECK(static_cast<bool>(frame));

    REQUIRE(frame.raw() != nullptr);
    REQUIRE(frame.raw()->rects != nullptr);
    CHECK(frame.raw()->num_rects == 2);
    CHECK(frame.raw()->rects[0]->type == SUBTITLE_TEXT);
    CHECK(frame.raw()->rects[1]->type == SUBTITLE_ASS);
    CHECK(std::string_view(frame.raw()->rects[0]->text) == "first");
    CHECK(std::string_view(frame.raw()->rects[1]->ass) == "Dialogue: second");

    auto rects = frame.rects();
    REQUIRE(rects.size() == 2);
    CHECK(rects[0].text() == "first");
    CHECK(rects[1].ass() == "Dialogue: second");

    CHECK(frame.rect(0).type() == SUBTITLE_TEXT);
    CHECK_FALSE(frame.rect(2).isValid());

    CHECK(frame.text() == "first");
    CHECK(frame.ass() == "Dialogue: second");

    frame.clear();
    CHECK(frame.numRects() == 0);
    CHECK_FALSE(frame.isValid());
    CHECK(frame.raw()->rects == nullptr);
}

TEST_CASE("SubtitleFrame: timing", "[Subtitle][Frame]")
{
    av::SubtitleFrame frame;
    CHECK(frame.timeBase() == av::Rational(1, AV_TIME_BASE));
    CHECK(frame.pts().isNoPts());

    frame.setPts(av::Timestamp(1000, av::Rational(1, 1000)));
    CHECK(frame.pts().timestamp(av::Rational(1, 1000)) == 1000);
    CHECK(frame.pts().timestamp(av::Rational(1, AV_TIME_BASE)) == 1000000);

    frame.setStartDisplayTime(500);
    frame.setEndDisplayTime(1500);
    CHECK(frame.startDisplayTime() == 500);
    CHECK(frame.endDisplayTime() == 1500);
    CHECK(frame.startTime().timestamp(av::Rational(1, 1000)) == 1500);
    CHECK(frame.endTime().timestamp(av::Rational(1, 1000)) == 2500);

    frame.setTimeBase(av::Rational(1, 1000));
    CHECK(frame.timeBase() == av::Rational(1, 1000));
    CHECK(frame.pts().timestamp() == 1000);

    frame.setTimeBase(av::Rational(1, 90000));
    CHECK(frame.pts().timestamp(av::Rational(1, 1000)) == 1000);

    frame.setPts(av::Timestamp());
    CHECK(frame.pts().isNoPts());
    CHECK(frame.startTime().isNoPts());
    CHECK(frame.endTime().isNoPts());
}

TEST_CASE("SubtitleFrame: move semantics", "[Subtitle][Frame]")
{
    av::SubtitleFrame source = av::SubtitleFrame::fromAss("Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,hello");
    CHECK(source.numRects() == 1);

    av::SubtitleFrame moved(std::move(source));
    CHECK(moved.numRects() == 1);
    CHECK(moved.ass().find("hello") != std::string_view::npos);
    CHECK(source.numRects() == 0);
    CHECK_FALSE(source.isValid());

    av::SubtitleFrame assigned = av::SubtitleFrame::fromText("old");
    assigned = std::move(moved);
    CHECK(assigned.numRects() == 1);
    CHECK(assigned.ass().find("hello") != std::string_view::npos);
    CHECK(moved.numRects() == 0);
    CHECK(moved.pts().isNoPts());
    CHECK(moved.timeBase() == av::Rational(1, AV_TIME_BASE));
    CHECK(moved.streamIndex() == -1);
    CHECK_FALSE(moved.isComplete());
}

TEST_CASE("SubtitleFrame: clone is a deep copy", "[Subtitle][Frame]")
{
    av::SubtitleFrame source;
    source.setFormat(1);
    source.setStartDisplayTime(100);
    source.setEndDisplayTime(200);
    source.setPts(av::Timestamp(42, av::Rational(1, 1000)));
    source.setStreamIndex(3);

    auto text = source.addRect(SUBTITLE_TEXT);
    text.setText("text payload");

    auto bitmap = source.addRect(SUBTITLE_BITMAP);
    auto plane = makeBitmap();
    bitmap.setBitmap(plane.data(), BITMAP_W, BITMAP_W, BITMAP_H);
    bitmap.setPalette(palette, 4);
    bitmap.setForced(true);

    auto clone = source.clone();
    REQUIRE(clone.numRects() == 2);
    CHECK(clone.format() == 1);
    CHECK(clone.startDisplayTime() == 100);
    CHECK(clone.endDisplayTime() == 200);
    CHECK(clone.pts().timestamp(av::Rational(1, 1000)) == 42);
    CHECK(clone.streamIndex() == 3);

    CHECK(clone.rect(0).text() == "text payload");
    CHECK(clone.rect(1).type() == SUBTITLE_BITMAP);
    CHECK(clone.rect(1).isForced());
    CHECK(clone.rect(1).nbColors() == 4);
    REQUIRE(clone.rect(1).data(0) != nullptr);
    CHECK(std::memcmp(clone.rect(1).data(0), plane.data(), plane.size()) == 0);
    CHECK(std::memcmp(clone.rect(1).data(1), palette, sizeof(palette)) == 0);

    CHECK(clone.rect(0).raw() != source.rect(0).raw());
    CHECK(clone.rect(1).data(0) != source.rect(1).data(0));
    CHECK(clone.rect(1).data(1) != source.rect(1).data(1));

    source.clear();
    CHECK(clone.numRects() == 2);
    CHECK(clone.rect(0).text() == "text payload");
    CHECK(clone.rect(1).nbColors() == 4);
}

TEST_CASE("SubtitleFrame: factories", "[Subtitle][Frame]")
{
    auto text = av::SubtitleFrame::fromText("plain text");
    REQUIRE(text.numRects() == 1);
    CHECK(text.rect(0).type() == SUBTITLE_TEXT);
    CHECK(text.text() == "plain text");

    auto ass = av::SubtitleFrame::fromAss("Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,ass text");
    REQUIRE(ass.numRects() == 1);
    CHECK(ass.rect(0).type() == SUBTITLE_ASS);
    CHECK(ass.ass().find("ass text") != std::string_view::npos);
}

TEST_CASE("SubtitleFrame: default ASS header", "[Subtitle]")
{
    auto header = av::subtitle::defaultAssHeader();
    CHECK(header.find("[Script Info]") != std::string_view::npos);
    CHECK(header.find("[V4+ Styles]") != std::string_view::npos);
    CHECK(header.find("[Events]") != std::string_view::npos);
    CHECK(header.find("Format: Layer, Start, End") != std::string_view::npos);
}

namespace {

constexpr const char *SRT_PACKET =
    "1\n"
    "00:00:00,000 --> 00:00:01,000\n"
    "Hello, avcpp!\n";

av::SubtitleDecoderContext makeSubripDecoder()
{
    auto codec = av::findDecodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleDecoderContext dec{codec};
    dec.setPktTimeBase(av::Rational(1, 1000));
    dec.open();
    return dec;
}

av::Packet makeSubripPacket()
{
    av::Packet packet(reinterpret_cast<const std::uint8_t*>(SRT_PACKET),
                      std::strlen(SRT_PACKET));
    packet.setTimeBase(av::Rational(1, 1000));
    packet.setPts(av::Timestamp(1234, av::Rational(1, 1000)));
    return packet;
}

} // namespace

TEST_CASE("SubtitleDecoderContext: decode SRT from memory", "[Subtitle][Codec]")
{
    auto dec = makeSubripDecoder();
    std::error_code ec;

    auto frame = dec.decode(makeSubripPacket(), ec);
    REQUIRE_FALSE(ec);
    REQUIRE(frame.isValid());
    CHECK(frame.isComplete());

    CHECK(frame.pts().timestamp(av::Rational(1, AV_TIME_BASE)) == 1234000);
    REQUIRE(frame.numRects() == 1);
    CHECK(frame.rect(0).type() == SUBTITLE_ASS);
    CHECK(frame.ass().find("Hello, avcpp!") != std::string_view::npos);
}

TEST_CASE("SubtitleDecoderContext: decode with an offset", "[Subtitle][Codec]")
{
    std::string payload = "leading garbage\n";
    payload += SRT_PACKET;

    av::SubtitleDecoderContext dec = makeSubripDecoder();
    av::Packet packet(reinterpret_cast<const std::uint8_t*>(payload.data()),
                      payload.size());
    packet.setTimeBase(av::Rational(1, 1000));
    packet.setPts(av::Timestamp(0, av::Rational(1, 1000)));

    std::error_code ec;
    size_t decodedBytes = 0;
    auto frame = dec.decode(packet, std::strlen("leading garbage\n"), decodedBytes, ec);
    CHECK_FALSE(ec);
    CHECK(decodedBytes > 0);
    REQUIRE(frame.isValid());
    CHECK(frame.ass().find("Hello, avcpp!") != std::string_view::npos);

    ec.clear();
    frame = dec.decode(packet, packet.size(), decodedBytes, ec);
    CHECK(ec == av::Errors::CodecDecodingOffsetToLarge);
}

TEST_CASE("SubtitleDecoderContext: pts requires a packet time base", "[Subtitle][Codec]")
{
    auto codec = av::findDecodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleDecoderContext dec{codec};
    dec.open();

    av::Packet packet(reinterpret_cast<const std::uint8_t*>(SRT_PACKET),
                      std::strlen(SRT_PACKET));
    packet.setTimeBase(av::Rational());
    packet.raw()->pts = 1234;

    std::error_code ec;
    auto frame = dec.decode(packet, ec);
    REQUIRE_FALSE(ec);
    REQUIRE(frame.isValid());
    CHECK(frame.pts().isNoPts());
}

TEST_CASE("SubtitleEncoderContext: SRT round trip", "[Subtitle][Codec]")
{
    av::SubtitleFrame frame = av::SubtitleFrame::fromAss(
        "Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,Hello, avcpp!");
    frame.setPts(av::Timestamp(1234, av::Rational(1, 1000)));
    frame.setEndDisplayTime(1000);

    auto codec = av::findEncodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 1000));
    enc.setDefaultSubtitleHeader();
    enc.open();

    std::error_code ec;
    auto packet = enc.encode(frame, ec);
    REQUIRE_FALSE(ec);
    REQUIRE(packet.size() > 0);
    CHECK(packet.isComplete());
    CHECK(packet.pts().timestamp(av::Rational(1, 1000)) == 1234);
    CHECK(packet.duration() == 1000);

    std::string payload(reinterpret_cast<const char*>(packet.data()), packet.size());
    CHECK(payload.find("Hello, avcpp!") != std::string::npos);
}

TEST_CASE("SubtitleEncoderContext: validation", "[Subtitle][Codec]")
{
    auto codec = av::findEncodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 1000));
    enc.setDefaultSubtitleHeader();
    enc.open();

    std::error_code ec;
    av::SubtitleFrame empty;
    auto packet = enc.encode(empty, ec);
    CHECK(ec == av::Errors::FrameInvalid);
    CHECK(packet.isNull());

    av::SubtitleFrame frame = av::SubtitleFrame::fromAss(
        "Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,text");
    frame.setStartDisplayTime(10);

    ec.clear();
    packet = enc.encode(frame, ec);
    CHECK(ec == av::Errors::InvalidArgument);
    CHECK(packet.isNull());
}

TEST_CASE("Subtitle codec contexts: generic send/receive/flush are rejected",
          "[Subtitle][Codec]")
{
    auto dec = makeSubripDecoder();

    std::error_code ec;
    av::Packet input = makeSubripPacket();
    CHECK_FALSE(static_cast<av::CodecContext2&>(dec).send(input, ec));
    CHECK(ec == av::Errors::CodecUnsupportedOperation);

    using DecoderBase = av::CodecContextBase<av::SubtitleDecoderContext,
                                             av::Direction::Decoding,
                                             AVMEDIA_TYPE_SUBTITLE>;
    ec.clear();
    CHECK_FALSE(static_cast<DecoderBase&>(dec).flush(ec));
    CHECK(ec == av::Errors::CodecUnsupportedOperation);

    auto codec = av::findEncodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());
    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 1000));
    enc.setDefaultSubtitleHeader();
    enc.open();

    av::SubtitleFrame frame = av::SubtitleFrame::fromAss(
        "Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,text");

    ec.clear();
    auto packet = static_cast<av::CodecContext2&>(enc).receive(ec);
    CHECK(ec == av::Errors::CodecUnsupportedOperation);

    using EncoderBase = av::CodecContextBase<av::SubtitleEncoderContext,
                                             av::Direction::Encoding,
                                             AVMEDIA_TYPE_SUBTITLE>;
    ec.clear();
    CHECK_FALSE(static_cast<EncoderBase&>(enc).flush(ec));
    CHECK(ec == av::Errors::CodecUnsupportedOperation);
}

TEST_CASE("Subtitle codec contexts: header accessors", "[Subtitle][Codec]")
{
    auto codec = av::findEncodingCodec(AV_CODEC_ID_SUBRIP);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 1000));

    CHECK(enc.subtitleHeader().empty());
    enc.setSubtitleHeader("header");
    CHECK(enc.subtitleHeader() == "header");

    enc.clearSubtitleHeader();
    CHECK(enc.subtitleHeader().empty());

    // subrip requires a header before open(). A failed open() resets the
    // FFmpeg context, so the successful attempt uses a fresh object.
    std::error_code ec;
    enc.open(ec);
    CHECK(ec);
    CHECK_FALSE(enc.isOpened());

    av::SubtitleEncoderContext enc2{codec};
    enc2.setTimeBase(av::Rational(1, 1000));
    enc2.setDefaultSubtitleHeader();
    CHECK(enc2.subtitleHeader().find("[Script Info]") != std::string_view::npos);
    enc2.open(ec);
    REQUIRE_FALSE(ec);
    CHECK(enc2.isOpened());

    ec.clear();
    enc2.setSubtitleHeader("another header", ec);
    CHECK(ec == av::Errors::CodecAlreadyOpened);
}

TEST_CASE("Subtitle codec contexts: ASS encoder opens without a header",
          "[Subtitle][Codec]")
{
    auto codec = av::findEncodingCodec(AV_CODEC_ID_ASS);
    REQUIRE_FALSE(codec.isNull());

    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 1000));

    std::error_code ec;
    enc.open(ec);
    CHECK_FALSE(ec);
    CHECK(enc.isOpened());
}
