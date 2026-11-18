// Lists hardware device types, probes the devices and prints the H.264 and
// H.265 hardware codec configurations.
//
// Usage: api2-hw-probe [device]
//
// The optional device string is backend-specific. For example, a VAAPI device
// is usually "/dev/dri/renderD128" and a CUDA device is a numeric index.

#include <iostream>
#include <string>

#include "avcpp/av.h"
#include "avcpp/codec.h"
#include "avcpp/hwcontext.h"

namespace {

const char* codecName(const av::Codec& codec)
{
    return codec.raw() ? codec.name() : "none";
}

std::string pixelFormatName(AVPixelFormat format)
{
    if (format == AV_PIX_FMT_NONE)
        return "none";

    std::error_code ec;
    const char* name = av::PixelFormat(format).name(ec);
    return ec ? "invalid" : name;
}

void printConfigs(const av::Codec& codec, const char* title)
{
    std::cout << "    " << title << " (" << codecName(codec) << "):";
    if (!codec.raw()) {
        std::cout << " none\n";
        return;
    }

    bool first = true;
    for (const AVCodecHWConfig& config : av::hw::codecConfigs(codec)) {
        std::cout << (first ? " " : ", ")
                  << av::hw::DeviceType(config.device_type).name()
                  << '/'
                  << pixelFormatName(config.pix_fmt);
        if (config.methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            std::cout << "+device";
        if (config.methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
            std::cout << "+frames";
        first = false;
    }
    if (first)
        std::cout << " none";
    std::cout << '\n';
}

void printCodecs(av::hw::DeviceType type)
{
    printConfigs(av::hw::findDecoder(AV_CODEC_ID_H264, type), "h264 decoder");
    printConfigs(av::hw::findEncoder(AV_CODEC_ID_H264, type), "h264 encoder");
    printConfigs(av::hw::findDecoder(AV_CODEC_ID_HEVC, type), "hevc decoder");
    printConfigs(av::hw::findEncoder(AV_CODEC_ID_HEVC, type), "hevc encoder");
}

} // namespace

int main(int argc, char** argv)
{
    av::init();

    const char* device = argc > 1 ? argv[1] : nullptr;

    std::cout << "Device types compiled into FFmpeg:";
    for (av::hw::DeviceType type : av::hw::allDeviceTypes())
        std::cout << ' ' << type.name();
    std::cout << "\n\n";

    for (const av::hw::DeviceProbe& probe : av::hw::probeDevices(device)) {
        std::cout << probe.type.name() << ": ";
        if (!probe.available) {
            std::cout << "unavailable (" << probe.error.message() << ")\n";
            continue;
        }
        std::cout << "available\n";

        std::error_code ec;
        av::hw::DeviceContext context =
            av::hw::DeviceContext::create(probe.type, device, {}, ec);
        if (!context.isValid()) {
            std::cout << "  device creation failed: " << ec.message() << '\n';
            continue;
        }

        av::hw::FrameConstraints constraints =
            av::hw::FrameConstraints::query(context, ec);
        if (constraints.isValid()) {
            std::cout << "  hardware formats:";
            for (av::PixelFormat format : constraints.hwFormats())
                std::cout << ' ' << format.name();
            std::cout << "\n  software formats:";
            for (av::PixelFormat format : constraints.swFormats())
                std::cout << ' ' << format.name();
            std::cout << "\n  size: "
                      << constraints.minWidth() << 'x' << constraints.minHeight()
                      << " .. "
                      << constraints.maxWidth() << 'x' << constraints.maxHeight()
                      << '\n';
        } else {
            std::cout << "  frame constraints: " << ec.message() << '\n';
        }

        printCodecs(probe.type);
    }

    return 0;
}
