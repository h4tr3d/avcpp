// Decodes the first video stream with a hardware decoder.
//
// Usage: api2-hw-decode <device> <input> [--to-system]
//
// <device> is a device type name such as "vaapi", "qsv", "cuda" or
// "videotoolbox". The optional --to-system flag transfers every decoded frame
// to system memory.

#include <cstring>
#include <iostream>
#include <string>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/hwcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <device> <input> [--to-system]\n";
        return 1;
    }

    av::init();

    const av::hw::DeviceType type = av::hw::DeviceType::fromName(argv[1]);
    if (type.isNone()) {
        std::cerr << "Unknown device type: " << argv[1] << '\n';
        return 1;
    }

    const bool toSystem = (argc > 3 && std::strcmp(argv[3], "--to-system") == 0);
    std::error_code ec;

    av::hw::DeviceContext device = av::hw::DeviceContext::create(type, nullptr, {}, ec);
    if (!device.isValid()) {
        std::cerr << "Can't create device: " << ec.message() << '\n';
        return 2;
    }

    av::FormatContext ictx;
    ictx.openInput(argv[2], ec);
    if (ec) {
        std::cerr << "Can't open input: " << ec.message() << '\n';
        return 1;
    }
    ictx.findStreamInfo(ec);
    if (ec) {
        std::cerr << "Can't find streams: " << ec.message() << '\n';
        return 1;
    }

    av::Stream vst;
    for (auto&& stream : ictx.streams()) {
        if (stream.mediaType() == AVMEDIA_TYPE_VIDEO) {
            vst = stream;
            break;
        }
    }
    if (!vst.isValid()) {
        std::cerr << "Video stream not found\n";
        return 1;
    }

    av::Codec codec = av::hw::findDecoder(vst.codecParameters().codecId(), type);
    if (!codec.raw()) {
        std::cerr << "No hardware decoder for this codec and device\n";
        return 2;
    }

    av::VideoDecoderContext decoder(vst, codec);
    decoder.setHwDeviceContext(device, ec);
    decoder.setHwTransferToSystem(toSystem);
    decoder.open(av::Codec(), ec);
    if (ec) {
        std::cerr << "Can't open decoder: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "decoder: " << codec.name() << ", device: " << type.name() << '\n';

    std::size_t packets = 0;
    std::size_t frames = 0;
    std::size_t hardware = 0;

    auto consume = [&](const av::VideoFrame& frame) {
        if (!frame)
            return;

        ++frames;
        if (frame.isHwFrame())
            ++hardware;

        if (frames == 1) {
            std::cout << "first frame: " << frame.width() << 'x' << frame.height()
                      << ", format: " << av::PixelFormat(static_cast<AVPixelFormat>(frame.raw()->format)).name()
                      << ", hardware: " << (frame.isHwFrame() ? "yes" : "no")
                      << '\n';
        }
    };

    av::Packet packet;
    while ((packet = ictx.readPacket(ec))) {
        if (packet.streamIndex() != vst.index())
            continue;

        ++packets;
        for (;;) {
            const bool accepted = decoder.send(packet, ec);
            if (ec) {
                std::cerr << "Send error: " << ec.message() << '\n';
                return 1;
            }

            for (;;) {
                av::VideoFrame frame = decoder.receive(ec);
                if (ec) {
                    std::cerr << "Receive error: " << ec.message() << '\n';
                    return 1;
                }
                if (!frame.isValid())
                    break;
                consume(frame);
            }

            if (accepted)
                break;
        }
    }
    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    // Hardware decoders can delay frames, so drain them after the last packet.
    if (!decoder.flush(ec)) {
        std::cerr << "Flush error: " << ec.message() << '\n';
        return 1;
    }
    for (;;) {
        av::VideoFrame frame = decoder.receive(ec);
        if (ec) {
            std::cerr << "Receive error: " << ec.message() << '\n';
            return 1;
        }
        if (!frame.isValid())
            break;
        consume(frame);
    }

    std::cout << "packets: " << packets
              << ", frames: " << frames
              << ", hardware frames: " << hardware << '\n';
    return 0;
}
