// Hardware device, frames and codec tests.
//
// The tests must never fail on a machine without hardware. A missing device,
// codec or backend is reported as SKIP. A device that exists but cannot be
// created or opened is a failure.
//
// Environment variables:
//   AVCPP_HW_DEVICES=vaapi,qsv  limit the device set
//   AVCPP_HW_SKIP=cuda          exclude unstable backends
//
// Parts that the current FFmpeg build cannot drive are skipped as well, see
// HardwareCompat.h.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "avcpp/avcompat.h"
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/frame.h"
#include "avcpp/hwcontext.h"
#include "avcpp/packet.h"

#include "HardwareCompat.h"

namespace {

bool isAvailable(av::hw::DeviceType type)
{
    return av::hw::isDeviceAvailable(type);
}

void fillNV12(av::VideoFrame& frame)
{
    AVFrame* raw = frame.raw();
    if (!raw || !raw->data[0])
        return;

    for (int row = 0; row < raw->height; ++row)
        std::memset(raw->data[0] + row * raw->linesize[0], 16, static_cast<std::size_t>(raw->width));

    if (raw->data[1]) {
        for (int row = 0; row < raw->height / 2; ++row)
            std::memset(raw->data[1] + row * raw->linesize[1], 128, static_cast<std::size_t>(raw->width));
    }
}

} // namespace

TEST_CASE("Hardware: device type names", "[hw][device-type]")
{
    CHECK(av::hw::DeviceType().isNone());
    CHECK(std::string(av::hw::DeviceType().name()) == "none");

    av::hw::DeviceType vaapi = av::hw::DeviceType::fromName("vaapi");
    if (!vaapi.isNone()) {
        CHECK(std::string(vaapi.name()) == "vaapi");
        CHECK(vaapi == av::hw::DeviceType::fromName(std::string("vaapi")));
    }

    CHECK(av::hw::DeviceType::fromName("no-such-device").isNone());
    CHECK(av::hw::DeviceType::fromName(static_cast<const char*>(nullptr)).isNone());
}

TEST_CASE("Hardware: range views are non-owning", "[hw][range]")
{
    int values[] = { 1, 2, 3 };
    av::hw::RangeView<int> view(values, 3);
    CHECK(view.size() == 3);
    CHECK_FALSE(view.empty());
    CHECK(view[1] == 2);
    CHECK(view.toVector() == std::vector<int>({ 1, 2, 3 }));

    const AVPixelFormat formats[] = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    av::hw::PixelFormatRange pixelFormats(formats);
    CHECK(pixelFormats.size() == 2);
    CHECK_FALSE(pixelFormats.empty());
    CHECK(pixelFormats.toVector() == std::vector<av::PixelFormat>({
              av::PixelFormat(AV_PIX_FMT_NV12),
              av::PixelFormat(AV_PIX_FMT_YUV420P) }));

    av::hw::PixelFormatList emptyList;
    CHECK(emptyList.empty());
    CHECK(emptyList.begin() == emptyList.end());

    for (av::hw::DeviceType type : av::hw::allDeviceTypes())
        CHECK_FALSE(type.isNone());
}

TEST_CASE("Hardware: codec configurations", "[hw][codec]")
{
    av::Codec h264 = av::findDecodingCodec(AV_CODEC_ID_H264);
    REQUIRE(h264.raw());

    av::hw::CodecConfigRange configs = av::hw::codecConfigs(h264);
    if (configs.empty())
        SKIP("This FFmpeg build has no hardware configuration for H.264");

    // The NONE device type never matches a hardware configuration.
    CHECK_FALSE(av::hw::findCodecConfig(h264, av::hw::DeviceType()).has_value());
    CHECK_FALSE(av::hw::findDecoder(AV_CODEC_ID_H264, av::hw::DeviceType()).raw());
    CHECK_FALSE(av::hw::findEncoder(AV_CODEC_ID_H264, av::hw::DeviceType()).raw());

    // The decoder list is a lazy range. It must agree with findDecoder().
    for (const std::string& name : avtest::selectedDevices()) {
        av::hw::DeviceType type = av::hw::DeviceType::fromName(name);
        if (type.isNone())
            continue;

        av::Codec decoder = av::hw::findDecoder(AV_CODEC_ID_H264, type);
        if (!decoder.raw())
            continue;

        bool found = false;
        for (av::Codec codec : av::hw::findDecoders(AV_CODEC_ID_H264, type)) {
            if (codec.raw() == decoder.raw()) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}

TEST_CASE("Hardware: frame constraints default state", "[hw][constraints]")
{
    av::hw::FrameConstraints constraints;
    CHECK_FALSE(constraints.isValid());
    CHECK(constraints.hwFormats().empty());
    CHECK(constraints.swFormats().empty());
    CHECK(constraints.minWidth() == 0);
    CHECK(constraints.maxWidth() == 0);
    CHECK(constraints.minHeight() == 0);
    CHECK(constraints.maxHeight() == 0);

    std::error_code ec;
    av::hw::FrameConstraints queried = av::hw::frameConstraints({}, ec);
    CHECK_FALSE(queried.isValid());
    CHECK(ec == av::make_error_code(av::Errors::HardwareDeviceInvalid));
}

TEST_CASE("Hardware: device probing agrees with creation", "[hw][probe]")
{
    bool anyFound = false;

    for (const std::string& name : avtest::selectedDevices()) {
        av::hw::DeviceType type = av::hw::DeviceType::fromName(name);
        if (type.isNone())
            continue;

        const bool available = isAvailable(type);

        for (const av::hw::DeviceProbe& probe : av::hw::probeDevices()) {
            if (probe.type.get() == type.get()) {
                anyFound = true;
                CHECK(probe.available == available);
            }
        }
    }

    if (!anyFound)
        SKIP("No selected device type is compiled into FFmpeg");
}

TEST_CASE("Hardware: device and frames contexts", "[hw][device]")
{
    bool anyAvailable = false;

    for (const std::string& name : avtest::selectedDevices()) {
        av::hw::DeviceType type = av::hw::DeviceType::fromName(name);
        if (type.isNone() || !isAvailable(type))
            continue;

        anyAvailable = true;

        DYNAMIC_SECTION("device " << name) {
            std::error_code ec;
            av::hw::DeviceContext device = av::hw::DeviceContext::create(type, nullptr, {}, ec);
            REQUIRE(device.isValid());
            CHECK(device.type().get() == type.get());
            CHECK(device.raw() != nullptr);
            CHECK(device.bufferRef() != nullptr);

            av::hw::FrameConstraints constraints = av::hw::frameConstraints(device, ec);
            REQUIRE(constraints.isValid());
            CHECK_FALSE(constraints.hwFormats().empty());
            CHECK_FALSE(constraints.swFormats().empty());

            const av::PixelFormat hwFormat = av::hw::hwPixelFormat(type);
            CHECK(hwFormat.get() != AV_PIX_FMT_NONE);

            // NV12 keeps fillNV12() correct for every backend that supports it.
            av::PixelFormat swFormat(AV_PIX_FMT_NV12);
            if (!constraints.swFormats().empty()) {
                swFormat = constraints.swFormats()[0];
                for (av::PixelFormat format : constraints.swFormats()) {
                    if (format.get() == AV_PIX_FMT_NV12) {
                        swFormat = format;
                        break;
                    }
                }
            }

            av::hw::FramesContext frames =
                av::hw::FramesContext::create(device, hwFormat, swFormat,
                                              320, 240, 0, ec);
            REQUIRE(frames.isValid());
            CHECK(frames.format().get() == hwFormat.get());
            CHECK(frames.swFormat().get() == swFormat.get());
            CHECK(frames.width() == 320);
            CHECK(frames.height() == 240);
            CHECK(frames.device().isValid());

            av::hw::PixelFormatList transfer =
                frames.transferFormats(AV_HWFRAME_TRANSFER_DIRECTION_FROM, ec);
            REQUIRE_FALSE(ec);
            CHECK_FALSE(transfer.empty());

            if (avtest::isQsvTransferBroken(type)) {
                WARN("Skipping frame transfer: broken with this FFmpeg and libmfx runtime");
                continue;
            }

            av::VideoFrame software(frames.swFormat(), 320, 240, 32);
            REQUIRE(software.isValid());
            software.setTimeBase(av::Rational{1, 25});
            software.setStreamIndex(3);
            fillNV12(software);

            av::VideoFrame hardware = software.toHwFrame(frames, ec);
            REQUIRE(hardware.isHwFrame());
            CHECK(hardware.hwFramesContext().isValid());
            CHECK(hardware.hwDeviceContext().isValid());
            CHECK(hardware.timeBase() == av::Rational(1, 25));
            CHECK(hardware.streamIndex() == 3);
            CHECK_THROWS_AS(hardware.clone(), av::Exception);

            std::error_code cloneEc;
            CHECK_FALSE(hardware.clone(1, cloneEc).isValid());
            CHECK(cloneEc == av::make_error_code(av::Errors::HardwareTransferUnsupported));

            av::VideoFrame systemMemory = hardware.toSystemMemory(AV_PIX_FMT_NONE, ec);
            REQUIRE_FALSE(systemMemory.isHwFrame());
            CHECK(systemMemory.width() == 320);
            CHECK(systemMemory.height() == 240);
            CHECK(systemMemory.timeBase() == av::Rational(1, 25));
            CHECK(systemMemory.streamIndex() == 3);

            av::VideoFrame mapped = hardware.mapTo(frames, AV_HWFRAME_MAP_READ, ec);
            if (ec) {
                // av_hwframe_map() is backend dependent and may report ENOSYS.
                CHECK_FALSE(mapped.isValid());
            } else {
                CHECK(mapped.isHwFrame());
                CHECK(mapped.timeBase() == av::Rational(1, 25));
                CHECK(mapped.streamIndex() == 3);
            }
        }
    }

    if (!anyAvailable)
        SKIP("No selected hardware device is available");
}

TEST_CASE("Hardware: encoder configuration and upload", "[hw][encode]")
{
    bool anyEncoder = false;

    for (const std::string& name : avtest::selectedDevices()) {
        av::hw::DeviceType type = av::hw::DeviceType::fromName(name);
        if (type.isNone() || !isAvailable(type))
            continue;

        av::Codec encoderCodec = av::hw::findEncoder(AV_CODEC_ID_H264, type);
        if (!encoderCodec.raw())
            continue;

        anyEncoder = true;

        if (avtest::isQsvEncoderUnavailable(type)) {
            WARN("Skipping " << name << " encoder: unavailable in this FFmpeg and libmfx runtime");
            continue;
        }
        DYNAMIC_SECTION("encoder " << name) {
            std::error_code ec;
            av::hw::DeviceContext device =
                av::hw::DeviceContext::create(type, nullptr, {}, ec);
            REQUIRE(device.isValid());

            av::VideoEncoderContext encoder(encoderCodec);
            encoder.setWidth(320);
            encoder.setHeight(240);
            encoder.setTimeBase(av::Rational{1, 25});
            encoder.setFrameRate(av::Rational{25, 1});
            encoder.setBitRate(500'000);

            encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                                        AV_PIX_FMT_NV12, ec);
            REQUIRE_FALSE(ec);
            CHECK(encoder.isHwEncoding());
            CHECK(encoder.hwDeviceContext().isValid());
            CHECK((encoder.hwEncodingMode() != av::hw::EncodingMode::Auto ||
                   encoder.hwFramesContext().isValid()));

            encoder.open(encoderCodec, ec);
            REQUIRE_FALSE(ec);

            av::PixelFormat softwareFormat(AV_PIX_FMT_NV12);
            if (encoder.hwFramesContext().isValid())
                softwareFormat = encoder.hwFramesContext().swFormat();

            std::size_t packets = 0;
            for (int index = 0; index < 5; ++index) {
                av::VideoFrame frame(softwareFormat, 320, 240, 32);
                REQUIRE(frame.isValid());
                fillNV12(frame);

                frame.setTimeBase(encoder.timeBase());
                frame.setPts(av::Timestamp(index, encoder.timeBase()));

                av::Packet packet = encoder.encode(frame, ec);
                REQUIRE_FALSE(ec);
                if (packet)
                    ++packets;
            }

            for (;;) {
                av::Packet packet = encoder.encode(ec);
                REQUIRE_FALSE(ec);
                if (!packet)
                    break;
                ++packets;
            }

            CHECK(packets > 0);
        }
    }

    if (!anyEncoder)
        SKIP("No hardware H.264 encoder is available");
}

TEST_CASE("Hardware: configuration errors", "[hw][errors]")
{
    std::error_code ec;
    av::Codec softwareEncoder = av::findEncodingCodec(std::string("libx264"));

    av::hw::DeviceType type;
    for (const std::string& name : avtest::selectedDevices()) {
        av::hw::DeviceType candidate = av::hw::DeviceType::fromName(name);
        if (!candidate.isNone() && isAvailable(candidate)) {
            type = candidate;
            break;
        }
    }

    if (type.isNone())
        SKIP("No selected hardware device is available");

    if (!softwareEncoder.raw())
        SKIP("No software H.264 encoder is available in this FFmpeg build");

    av::hw::DeviceContext device = av::hw::DeviceContext::create(type, nullptr, {}, ec);
    REQUIRE(device.isValid());

    {
        av::VideoEncoderContext encoder(softwareEncoder);
        encoder.setWidth(320);
        encoder.setHeight(240);
        encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                                    AV_PIX_FMT_NV12, ec);
        CHECK(ec == av::make_error_code(av::Errors::HardwareConfigNotFound));
    }

    av::VideoDecoderContext decoder(av::findDecodingCodec(AV_CODEC_ID_H264));
    REQUIRE(decoder.isValid());
    decoder.open(ec);
    REQUIRE_FALSE(ec);

    decoder.setHwDeviceContext(device, ec);
    CHECK(ec == av::make_error_code(av::Errors::CodecAlreadyOpened));

    av::VideoEncoderContext invalidEncoder(softwareEncoder);
    invalidEncoder.setHwDeviceContext(av::hw::DeviceContext(), ec);
    CHECK(ec == av::make_error_code(av::Errors::HardwareDeviceInvalid));

    // A decoder without a hardware configuration for the device type must not
    // open: there is no silent fallback.
    av::Codec nativeDecoder = av::findDecodingCodec(AV_CODEC_ID_H264);
    const int anyHwMethod = AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX |
                            AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX;
    if (nativeDecoder.raw() &&
        !av::hw::findCodecConfig(nativeDecoder, type, anyHwMethod).has_value()) {
        av::VideoDecoderContext softwareDecoder(nativeDecoder);
        softwareDecoder.setHwDeviceContext(device, ec);
        REQUIRE_FALSE(ec);
        softwareDecoder.open(ec);
        CHECK(ec == av::make_error_code(av::Errors::HardwareConfigNotFound));
    }
}
