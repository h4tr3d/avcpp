#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Patterns.hpp"

#include "avcpp/averror.h"
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/frame.h"
#include "avcpp/packet.h"
#include "avcpp/timestamp.h"

namespace {

constexpr int W = 128;
constexpr int H = 96;
constexpr int FRAMES = 8;

constexpr int AUDIO_RATE = 48000;
constexpr int AUDIO_SAMPLES = 1024;
constexpr int AUDIO_FRAMES = 4;

const av::Rational TB{1, 25};

av::VideoDecoderContext makeRawVideoDecoder()
{
    auto codec = av::findDecodingCodec(AV_CODEC_ID_RAWVIDEO);
    REQUIRE_FALSE(codec.isNull());

    av::VideoDecoderContext dec{codec};
    dec.setWidth(W);
    dec.setHeight(H);
    dec.setPixelFormat(AV_PIX_FMT_GRAY8);
    dec.setTimeBase(TB);
    dec.open();
    return dec;
}

av::VideoEncoderContext makeMpeg4Encoder()
{
    auto codec = av::findEncodingCodec(AV_CODEC_ID_MPEG4);
    REQUIRE_FALSE(codec.isNull());

    av::VideoEncoderContext enc{codec};
    enc.setWidth(W);
    enc.setHeight(H);
    enc.setPixelFormat(AV_PIX_FMT_YUV420P);
    enc.setTimeBase(TB);
    enc.setGopSize(4);
    enc.open();
    return enc;
}

av::VideoFrame makeVideoFrame(int index)
{
    av::VideoFrame frame(AV_PIX_FMT_YUV420P, W, H);
    avcpp_test::fillVideoFrame(frame, index);
    frame.setPts(av::Timestamp{index, TB});
    return frame;
}

} // namespace

TEST_CASE("Send/receive: decoder returns an empty result before any input")
{
    auto dec = makeRawVideoDecoder();
    std::error_code ec;

    auto frame = dec.receive(ec);
    CHECK_FALSE(ec);
    CHECK_FALSE(frame.isValid());
    CHECK_FALSE(frame.isComplete());
    CHECK_FALSE(dec.isEof());
}

TEST_CASE("Send/receive: rawvideo decodes one frame per packet")
{
    auto dec = makeRawVideoDecoder();
    std::error_code ec;

    std::vector<std::uint8_t> block(static_cast<std::size_t>(W) * H, 0x7F);

    int frames = 0;
    for (int i = 0; i < FRAMES; ++i) {
        av::Packet pkt{block, av::Packet::wrap_data_static{}};
        pkt.setTimeBase(TB);
        pkt.setPts(av::Timestamp{i, TB});

        REQUIRE(dec.send(pkt, ec));
        CHECK_FALSE(ec);

        for (;;) {
            auto frame = dec.receive(ec);
            CHECK_FALSE(ec);
            if (!frame.isValid())
                break;

            CHECK(frame.isComplete());
            ++frames;
        }

        CHECK_FALSE(dec.isEof());
    }

    CHECK(frames == FRAMES);
}

TEST_CASE("Send/receive: an empty packet switches the decoder to draining")
{
    auto dec = makeRawVideoDecoder();
    std::error_code ec;

    CHECK(dec.send(av::Packet{}, ec));
    CHECK_FALSE(ec);

    for (;;) {
        auto frame = dec.receive(ec);
        CHECK_FALSE(ec);
        if (!frame.isValid())
            break;
    }

    CHECK(dec.isEof());
}

TEST_CASE("Send/receive: flush drains the decoder and sets EOF")
{
    auto dec = makeRawVideoDecoder();
    std::error_code ec;

    REQUIRE(dec.flush(ec));
    CHECK_FALSE(ec);

    int frames = 0;
    for (;;) {
        auto frame = dec.receive(ec);
        CHECK_FALSE(ec);
        if (!frame.isValid())
            break;
        ++frames;
    }

    CHECK(frames == 0);
    CHECK(dec.isEof());
}

TEST_CASE("Send/receive: EOF state is stable and resettable")
{
    auto dec = makeRawVideoDecoder();
    std::error_code ec;

    REQUIRE(dec.flush(ec));
    CHECK_FALSE(ec);

    auto frame = dec.receive(ec);
    CHECK_FALSE(ec);
    CHECK_FALSE(frame.isValid());
    REQUIRE(dec.isEof());

    // Repeated receive after EOF stays empty and does not throw.
    frame = dec.receive(ec);
    CHECK_FALSE(ec);
    CHECK_FALSE(frame.isValid());
    CHECK(dec.isEof());

    CHECK_NOTHROW(dec.receive());
    CHECK(dec.isEof());

    // flush() after EOF is a no-op.
    CHECK(dec.flush(ec));
    CHECK_FALSE(ec);
    CHECK(dec.isEof());

    // flushBuffers() resets both the codec state and the EOF flag.
    dec.flushBuffers();
    CHECK_FALSE(dec.isEof());

    frame = dec.receive(ec);
    CHECK_FALSE(ec);
    CHECK_FALSE(frame.isValid());
    CHECK_FALSE(dec.isEof());
}

TEST_CASE("Send/receive: unopened context reports CodecNotOpened")
{
    auto codec = av::findDecodingCodec(AV_CODEC_ID_RAWVIDEO);
    REQUIRE_FALSE(codec.isNull());

    av::VideoDecoderContext dec{codec};
    std::error_code ec;

    auto frame = dec.receive(ec);
    CHECK_FALSE(frame.isValid());
    CHECK(ec == make_error_code(av::Errors::CodecNotOpened));

    ec.clear();
    CHECK_FALSE(dec.flush(ec));
    CHECK(ec == make_error_code(av::Errors::CodecNotOpened));
}

TEST_CASE("Send/receive: direction is validated")
{
    SECTION("decode API on an encoder")
    {
        auto enc = makeMpeg4Encoder();
        av::CodecContext2 &base = enc;
        std::error_code ec;

        av::Packet dummy;
        CHECK_FALSE(base.send(dummy, ec));
        CHECK(ec == make_error_code(av::Errors::CodecInvalidDirection));
    }

    SECTION("encode API on a decoder")
    {
        auto dec = makeRawVideoDecoder();
        av::CodecContext2 &base = dec;
        std::error_code ec;

        auto pkt = base.receive(ec);
        CHECK(pkt.isNull());
        CHECK(ec == make_error_code(av::Errors::CodecInvalidDirection));
    }
}

TEST_CASE("Send/receive: encoder emits one packet per frame")
{
    auto enc = makeMpeg4Encoder();
    std::error_code ec;

    int packets = 0;
    for (int i = 0; i < FRAMES; ++i) {
        auto frame = makeVideoFrame(i);
        for (;;) {
            bool accepted = enc.send(frame, ec);
            CHECK_FALSE(ec);

            for (;;) {
                auto pkt = enc.receive(ec);
                CHECK_FALSE(ec);
                if (pkt.isNull())
                    break;

                CHECK(pkt.isComplete());
                ++packets;
            }

            if (accepted)
                break;
        }

        CHECK_FALSE(enc.isEof());
    }

    REQUIRE(enc.flush(ec));
    CHECK_FALSE(ec);

    for (;;) {
        auto pkt = enc.receive(ec);
        CHECK_FALSE(ec);
        if (pkt.isNull())
            break;
        ++packets;
    }

    CHECK(enc.isEof());
    CHECK(packets == FRAMES);
}

TEST_CASE("Send/receive: encoder returns an empty result before any input")
{
    auto enc = makeMpeg4Encoder();
    std::error_code ec;

    auto pkt = enc.receive(ec);
    CHECK_FALSE(ec);
    CHECK(pkt.isNull());
    CHECK_FALSE(pkt.isComplete());
    CHECK_FALSE(enc.isEof());
}

TEST_CASE("Send/receive: encode/decode round trip")
{
    std::error_code ec;

    auto enc = makeMpeg4Encoder();
    std::vector<av::Packet> packets;
    for (int i = 0; i < FRAMES; ++i) {
        auto frame = makeVideoFrame(i);
        for (;;) {
            bool accepted = enc.send(frame, ec);
            CHECK_FALSE(ec);

            for (;;) {
                auto pkt = enc.receive(ec);
                CHECK_FALSE(ec);
                if (pkt.isNull())
                    break;
                packets.push_back(pkt);
            }

            if (accepted)
                break;
        }
    }

    REQUIRE(enc.flush(ec));
    for (;;) {
        auto pkt = enc.receive(ec);
        CHECK_FALSE(ec);
        if (pkt.isNull())
            break;
        packets.push_back(pkt);
    }

    CHECK(enc.isEof());
    REQUIRE(packets.size() == std::size_t(FRAMES));
    CHECK(packets.front().isKeyPacket());

    auto codec = av::findDecodingCodec(AV_CODEC_ID_MPEG4);
    REQUIRE_FALSE(codec.isNull());

    av::VideoDecoderContext dec{codec};
    dec.setTimeBase(TB);
    dec.open(ec);
    REQUIRE_FALSE(ec);

    int frames = 0;
    for (auto &pkt : packets) {
        for (;;) {
            bool accepted = dec.send(pkt, ec);
            CHECK_FALSE(ec);

            for (;;) {
                auto frame = dec.receive(ec);
                CHECK_FALSE(ec);
                if (!frame.isValid())
                    break;

                CHECK(frame.isComplete());
                ++frames;
            }

            if (accepted)
                break;
        }
    }

    REQUIRE(dec.flush(ec));
    for (;;) {
        auto frame = dec.receive(ec);
        CHECK_FALSE(ec);
        if (!frame.isValid())
            break;
        ++frames;
    }

    CHECK(dec.isEof());
    CHECK(frames == FRAMES);
}

TEST_CASE("Send/receive: PCM audio round trip")
{
    std::error_code ec;

    auto encCodec = av::findEncodingCodec(AV_CODEC_ID_PCM_S16LE);
    REQUIRE_FALSE(encCodec.isNull());

    av::AudioEncoderContext enc{encCodec};
    enc.setSampleRate(AUDIO_RATE);
    enc.setChannels(2);
    enc.setSampleFormat(AV_SAMPLE_FMT_S16);
    enc.setTimeBase({1, AUDIO_RATE});
    enc.open(ec);
    REQUIRE_FALSE(ec);

    std::vector<av::Packet> packets;
    double phase = 0.0;
    for (int i = 0; i < AUDIO_FRAMES; ++i) {
        av::AudioSamples samples(AV_SAMPLE_FMT_S16, AUDIO_SAMPLES,
                                 AV_CH_LAYOUT_STEREO, AUDIO_RATE);
        REQUIRE(samples.isValid());

        avcpp_test::fillAudioSine(samples, 440.0, phase);
        samples.setPts(av::Timestamp{i * AUDIO_SAMPLES, {1, AUDIO_RATE}});

        for (;;) {
            bool accepted = enc.send(samples, ec);
            CHECK_FALSE(ec);

            for (;;) {
                auto pkt = enc.receive(ec);
                CHECK_FALSE(ec);
                if (pkt.isNull())
                    break;
                packets.push_back(pkt);
            }

            if (accepted)
                break;
        }
    }

    REQUIRE(enc.flush(ec));
    for (;;) {
        auto pkt = enc.receive(ec);
        CHECK_FALSE(ec);
        if (pkt.isNull())
            break;
        packets.push_back(pkt);
    }

    CHECK(enc.isEof());
    REQUIRE(packets.size() == std::size_t(AUDIO_FRAMES));

    auto decCodec = av::findDecodingCodec(AV_CODEC_ID_PCM_S16LE);
    REQUIRE_FALSE(decCodec.isNull());

    av::AudioDecoderContext dec{decCodec};
    dec.setSampleRate(AUDIO_RATE);
    dec.setChannels(2);
    dec.setSampleFormat(AV_SAMPLE_FMT_S16);
    dec.setTimeBase({1, AUDIO_RATE});
    dec.open(ec);
    REQUIRE_FALSE(ec);

    int totalSamples = 0;
    for (auto &pkt : packets) {
        REQUIRE(dec.send(pkt, ec));
        CHECK_FALSE(ec);

        for (;;) {
            auto samples = dec.receive(ec);
            CHECK_FALSE(ec);
            if (!samples.isValid())
                break;

            CHECK(samples.isComplete());
            totalSamples += samples.samplesCount();
        }
    }

    REQUIRE(dec.flush(ec));
    for (;;) {
        auto samples = dec.receive(ec);
        CHECK_FALSE(ec);
        if (!samples.isValid())
            break;
        totalSamples += samples.samplesCount();
    }

    CHECK(dec.isEof());
    CHECK(totalSamples == AUDIO_FRAMES * AUDIO_SAMPLES);
}
