#include <catch2/catch_test_macros.hpp>
#include "catch2/catch_message.hpp"

#include "avcpp/format.h"
#include "avcpp/formatcontext.h"

#if AVCPP_HAS_AVFORMAT && (AVCPP_CXX_STANDARD >= 20)

static constexpr std::size_t     ImageW = 640;
static constexpr std::size_t     ImageH = 480;
static constexpr std::size_t     ImageCount = 11;
static constexpr av::PixelFormat ImagePixFmt = AV_PIX_FMT_GRAY8;

static const std::size_t FrameSizeBytes = ImageW * ImageH * ImagePixFmt.bitsPerPixel() / 8;

struct TestBufferIo : public av::CustomIO
{
public:
    std::vector<uint8_t> _buffer{};
    std::vector<uint8_t>::iterator _pos{};

    TestBufferIo()
    {
        _buffer.resize(FrameSizeBytes * ImageCount);
        _pos = _buffer.begin();
    }

    std::size_t remain() const noexcept
    {
        return std::ranges::distance(_pos, _buffer.end());
    }

    bool eof() const noexcept
    {
        return _pos == _buffer.end();
    }

    void fillPattern(uint8_t patternOffset)
    {
        std::span out = _buffer;
        for (auto i = 0u; i < ImageCount; ++i) {
            auto buf = out.subspan(i * FrameSizeBytes, FrameSizeBytes);
            std::ranges::fill(buf, uint8_t(i + patternOffset));
        }
    }

    void fill(uint8_t val = 0xFF)
    {
        std::ranges::fill(_buffer, val);
    }

    //
    // Iface
    //

    int write(const uint8_t *data, size_t size) final
    {
        UNSCOPED_INFO("IO::write: size=" << size);
        // ENOSPC
        // EWOULDBLOCK
        // EMSGSIZE
        // Do not write
        if (size > remain()) {
            UNSCOPED_INFO("IO::write too big: " << size);
            return AVERROR(EMSGSIZE);
        }

        std::ranges::copy_n(data, size, _pos);
        std::advance(_pos, size);

        return 0;
    }

    int read(uint8_t *data, size_t size) final
    {
        UNSCOPED_INFO("IO::read: size=" << size);
        if (eof()) {
            INFO("IO::read: EOF reached");
            return AVERROR_EOF;
        }

        auto readed = std::min(size, remain());
        std::ranges::copy_n(_pos, readed, data);
        std::advance(_pos, readed);
        return readed;
    }

    int64_t seek(int64_t offset, int whence) final
    {
        UNSCOPED_INFO("IO::seek: offset=" << offset << ", whence=0x" << std::hex << whence << std::dec);

        if (whence & AVSEEK_SIZE) {
            return _buffer.size();
        }

        ssize_t cur = -1;

        if (whence == SEEK_CUR) {
            cur = std::distance(_buffer.begin(), _pos);
        } else if (whence == SEEK_SET) {
            cur = 0;
        } else if (whence == SEEK_END) {
            cur = _buffer.size() - 1;
        } else {
            AVERROR(EINVAL);
        }

        assert(cur >= 0 && cur <= _buffer.size() - 1);

        auto next = cur + offset;
        if (next >= _buffer.size() || next < 0)
            return AVERROR(EINVAL);
        std::advance(_pos, offset);
        return next;
    }

    int seekable() const final
    {
        return AVIO_SEEKABLE_NORMAL;
    }

    const char *name() const final
    {
        return "TestBufferIo";
    }
};


TEST_CASE("Format Custom IO checks", "[FormatCustomIo]")
{
    SECTION("CustomIo :: Basic")
    {
        TestBufferIo customIoIn;
        TestBufferIo customIoOut;
        static auto const VideoSize = std::format("{}x{}", ImageW, ImageH);
        static auto const FrameSizeBytes = ImageW * ImageH * ImagePixFmt.bitsPerPixel() / 8;
        static auto const PatternOffset = 100u;

        // Fill In buffer with pattern
        customIoIn.fillPattern(PatternOffset);

        // Fill Out buffer with FF
        customIoOut.fill();

        {
            av::FormatContext ictx;
            ictx.openInput(&customIoIn,
                           av::Dictionary {
                               {"pixel_format", ImagePixFmt.name()},
                               {"video_size",   VideoSize.c_str()},
                           },
                           av::InputFormat("rawvideo"));
            ictx.findStreamInfo();
            std::size_t count = 0;
            while (auto pkt = ictx.readPacket()) {
                INFO("Pkt counter: " << count << ", pattern value " << (count + PatternOffset));
                REQUIRE(std::ranges::find_if_not(pkt.span(), [&](auto val) { return val == count + PatternOffset; }) == pkt.span().end());
                ++count;
            }
            REQUIRE(ictx.streamsCount() == 1);
            REQUIRE(ImageCount == count);
        }

        {
            av::FormatContext octx;
            octx.setFormat(av::OutputFormat("rawvideo"));
            octx.addStream();
            octx.openOutput(&customIoOut);
            octx.writeHeader(av::Dictionary {
                {"pixel_format", ImagePixFmt.name()},
                {"video_size",   VideoSize.c_str()},
            });

            std::vector<uint8_t> packetData(FrameSizeBytes);

            for (auto i = 0u; i < ImageCount; ++i) {
                std::ranges::fill(packetData, uint8_t(i + PatternOffset));
                av::Packet pkt{packetData, av::Packet::wrap_data_static{}};
                pkt.setPts(av::Timestamp{std::chrono::seconds(i)});
                pkt.setStreamIndex(0);
                octx.writePacket(pkt);
            }
            octx.flush();

            // Check Output data
            std::span in =  customIoOut._buffer;
            for (auto i = 0u; i < ImageCount; ++i) {
                auto buf = in.subspan(i * FrameSizeBytes, FrameSizeBytes);
                INFO("Pkt counter: " << i << ", pattern value " << (i + PatternOffset));
                REQUIRE(std::ranges::find_if_not(buf, [&](auto val) { return val == i + PatternOffset; }) == buf.end());
            }
        }
    }

    SECTION("Output Open")
    {
        TestBufferIo customIo;
        static auto const VideoSize = std::format("{}x{}", ImageW, ImageH);
        static auto const FrameSizeBytes = ImageW * ImageH * ImagePixFmt.bitsPerPixel() / 8;
        static auto const PatternOffset = 100u;

        {
            av::FormatContext ctx;
            CHECK_THROWS(ctx.openOutput(&customIo,
                                        av::Dictionary {
                                            {"pixel_format", ImagePixFmt.name()},
                                            {"video_size",   VideoSize.c_str()},
                                            }));
            REQUIRE(ctx.isOpened() == false);
        }

        {
            av::FormatContext ctx;
            ctx.setFormat(av::OutputFormat{"rawvideo"});

            // Yep, format pointed, but there is no any streams for muxing and initOuput called.
            REQUIRE_THROWS(ctx.openOutput(&customIo,
                                          av::Dictionary {
                                              {"pixel_format", ImagePixFmt.name()},
                                              {"video_size",   VideoSize.c_str()},
                                              }));

            REQUIRE(ctx.isOpened() == false);

            // set format again
            ctx.setFormat(av::OutputFormat{"rawvideo"});
            // add stream for muxing, it must be valid now
            REQUIRE_NOTHROW(ctx.addStream());
            REQUIRE_NOTHROW(ctx.openOutput(&customIo,
                                           av::Dictionary {
                                               {"pixel_format", ImagePixFmt.name()},
                                               {"video_size",   VideoSize.c_str()},
                                               }));
        }

        {
            av::FormatContext ctx;
            // no streams, should throws
            CHECK_THROWS(ctx.openOutput(&customIo,
                                        av::Dictionary {
                                            {"pixel_format", ImagePixFmt.name()},
                                            {"video_size",   VideoSize.c_str()},
                                            },
                                        av::OutputFormat{"rawvideo"}));
            REQUIRE(ctx.isOpened() == false);

            ctx.addStream();

            CHECK_NOTHROW(ctx.openOutput(&customIo,
                                        av::Dictionary {
                                            {"pixel_format", ImagePixFmt.name()},
                                            {"video_size",   VideoSize.c_str()},
                                            },
                                        av::OutputFormat{"rawvideo"}));
            REQUIRE(ctx.isOpened() == true);
        }

        // non-move stor for options
        {
            av::FormatContext ctx;

            av::Dictionary options {
                {"pixel_format", ImagePixFmt.name()},
                {"video_size",   VideoSize.c_str()}
            };

            // no streams, should throws
            CHECK_THROWS(ctx.openOutput(&customIo,
                                        options,
                                        av::OutputFormat{"rawvideo"}));
            REQUIRE(ctx.isOpened() == false);

            // Options should kept:
            REQUIRE(options.count() == 2);

            ctx.addStream();
            CHECK_NOTHROW(ctx.openOutput(&customIo,
                                         options,
                                         av::OutputFormat{"rawvideo"}));
            REQUIRE(ctx.isOpened() == true);

            // Options should be empty
            // REQUIRE(options.count() == 0);
        }

        // Check write
        {
            av::FormatContext ctx;

            av::Dictionary options {
                {"pixel_format", ImagePixFmt.name()},
                {"video_size",   VideoSize.c_str()}
            };

            // Options should kept:
            REQUIRE(options.count() == 2);

            ctx.addStream();
            bool openOutputFlag{};
            CHECK_NOTHROW(openOutputFlag = ctx.openOutput(&customIo,
                                                          options,
                                                          av::OutputFormat{"rawvideo"}));
            REQUIRE(ctx.isOpened() == true);
            // for the rawvideo openOutputFlag should be false
            REQUIRE(openOutputFlag == false);

            ctx.writeHeader();

            // fill with FF
            customIo.fill();

            std::vector<uint8_t> packetData(FrameSizeBytes);

            for (auto i = 0u; i < ImageCount; ++i) {
                std::ranges::fill(packetData, uint8_t(i + PatternOffset));
                av::Packet pkt{packetData, av::Packet::wrap_data_static{}};
                pkt.setPts(av::Timestamp{std::chrono::seconds(i)});
                pkt.setStreamIndex(0);
                ctx.writePacket(pkt);
            }
            ctx.flush();

            // Check Output data
            std::span in =  customIo._buffer;
            for (auto i = 0u; i < ImageCount; ++i) {
                auto buf = in.subspan(i * FrameSizeBytes, FrameSizeBytes);
                INFO("Pkt counter: " << i << ", pattern value " << (i + PatternOffset));
                REQUIRE(std::ranges::find_if_not(buf, [&](auto val) { return val == i + PatternOffset; }) == buf.end());
            }
        }
    }
}

#endif