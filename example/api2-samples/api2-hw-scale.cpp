// Full hardware pipeline with a software rescale step.
//
// Usage: api2-hw-scale <device> <input> <output>
//
// The sample decodes on the GPU, transfers every frame to system memory,
// rescales it with libswscale and uploads it back to the GPU for encoding.
// The output is half the input size.

#include <iostream>
#include <memory>
#include <string>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/format.h"
#include "avcpp/formatcontext.h"
#include "avcpp/hwcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"
#include "avcpp/videorescaler.h"

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <device> <input> <output>\n";
        return 1;
    }

    av::init();

    const av::hw::DeviceType type = av::hw::DeviceType::fromName(argv[1]);
    if (type.isNone()) {
        std::cerr << "Unknown device type: " << argv[1] << '\n';
        return 1;
    }

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

    const AVCodecID codecId = vst.codecParameters().codecId();

    av::Codec decoderCodec = av::hw::findDecoder(codecId, type);
    av::Codec encoderCodec = av::hw::findEncoder(codecId, type);
    if (!decoderCodec.raw() || !encoderCodec.raw()) {
        std::cerr << "No hardware decoder or encoder for this codec and device\n";
        return 2;
    }

    av::VideoDecoderContext decoder(vst, decoderCodec);
    decoder.setHwDeviceContext(device, ec);
    decoder.open(av::Codec(), ec);
    if (ec) {
        std::cerr << "Can't open decoder: " << ec.message() << '\n';
        return 1;
    }

    const int dstWidth  = (decoder.width() / 2) & ~1;
    const int dstHeight = (decoder.height() / 2) & ~1;
    if (dstWidth <= 0 || dstHeight <= 0) {
        std::cerr << "Input frame is too small to rescale\n";
        return 1;
    }

    av::OutputFormat ofrmt;
    ofrmt.setFormat(std::string(), argv[3]);
    av::FormatContext octx;
    octx.setFormat(ofrmt);

    av::VideoEncoderContext encoder(encoderCodec);
    encoder.setWidth(dstWidth);
    encoder.setHeight(dstHeight);
    encoder.setTimeBase(av::Rational{1, 1000});
    encoder.setFrameRate(vst.frameRate());
    encoder.setBitRate(decoder.bitRate());
    if (octx.outputFormat().isFlags(AVFMT_GLOBALHEADER))
        encoder.addFlags(AV_CODEC_FLAG_GLOBAL_HEADER);

    // Auto mode creates a hardware frames context. Its software format is the
    // format that the rescaler must produce for the upload.
    encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                                AV_PIX_FMT_NONE, ec);
    if (ec) {
        std::cerr << "Can't configure hardware encoding: " << ec.message() << '\n';
        return 1;
    }

    const av::hw::FramesContext hwFrames = encoder.hwFramesContext();
    if (!hwFrames.isValid()) {
        std::cerr << "Hardware frames context is not available\n";
        return 1;
    }

    encoder.open(encoderCodec, ec);
    if (ec) {
        std::cerr << "Can't open encoder: " << ec.message() << '\n';
        return 1;
    }

    av::Stream ost = octx.addStream(encoder);
    ost.setFrameRate(vst.frameRate());
    ost.setAverageFrameRate(vst.frameRate());
    ost.setTimeBase(encoder.timeBase());

    octx.openOutput(argv[3], ec);
    if (ec) {
        std::cerr << "Can't open output: " << ec.message() << '\n';
        return 1;
    }
    octx.writeHeader(ec);
    octx.flush();
    if (ec) {
        std::cerr << "Can't write header: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "pipeline: " << decoderCodec.name() << " -> system memory -> "
              << dstWidth << 'x' << dstHeight << " -> " << encoderCodec.name() << '\n';

    std::unique_ptr<av::VideoRescaler> rescaler;
    std::size_t frames = 0;
    int64_t index = 0;

    // Decoded frame -> system memory -> rescale -> hardware upload -> encode.
    auto process = [&](av::VideoFrame hardware) -> bool {
        const av::Timestamp sourceTimestamp = hardware.pts();

        av::VideoFrame software = hardware.toSystemMemory(AV_PIX_FMT_NONE, ec);
        if (ec) {
            std::cerr << "Can't transfer frame to system memory: "
                      << ec.message() << '\n';
            return false;
        }

        if (!rescaler) {
            rescaler = std::make_unique<av::VideoRescaler>(
                dstWidth, dstHeight, hwFrames.swFormat(),
                software.width(), software.height(), software.pixelFormat());
        }

        av::VideoFrame scaled = rescaler->rescale(software, ec);
        if (ec) {
            std::cerr << "Rescale error: " << ec.message() << '\n';
            return false;
        }

        av::VideoFrame uploaded = scaled.toHwFrame(hwFrames, ec);
        if (ec) {
            std::cerr << "Can't upload frame to hardware: " << ec.message() << '\n';
            return false;
        }

        // The transfer and rescale create new wrapper objects, so assign the
        // output timing explicitly. This keeps the encoder timestamps valid.
        uploaded.setTimeBase(encoder.timeBase());
        uploaded.setStreamIndex(0);
        if (sourceTimestamp.isValid() && !sourceTimestamp.isNoPts())
            uploaded.setPts(sourceTimestamp);
        else
            uploaded.setPts(av::Timestamp(index, encoder.timeBase()));
        ++index;

        av::Packet output = encoder.encode(uploaded, ec);
        if (ec) {
            std::cerr << "Encode error: " << ec.message() << '\n';
            return false;
        }
        if (!output)
            return true;

        output.setStreamIndex(0);
        octx.writePacket(output, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << '\n';
            return false;
        }

        ++frames;
        return true;
    };

    av::Packet packet;
    while ((packet = ictx.readPacket(ec))) {
        if (packet.streamIndex() != vst.index())
            continue;

        for (;;) {
            const bool accepted = decoder.send(packet, ec);
            if (ec) {
                std::cerr << "Send error: " << ec.message() << '\n';
                return 1;
            }

            for (;;) {
                av::VideoFrame hardware = decoder.receive(ec);
                if (ec) {
                    std::cerr << "Receive error: " << ec.message() << '\n';
                    return 1;
                }
                if (!hardware.isValid())
                    break;
                if (!process(std::move(hardware)))
                    return 1;
            }

            if (accepted)
                break;
        }
    }
    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    // Hardware decoders delay frames, so drain them after the last packet.
    if (!decoder.flush(ec)) {
        std::cerr << "Flush error: " << ec.message() << '\n';
        return 1;
    }
    for (;;) {
        av::VideoFrame hardware = decoder.receive(ec);
        if (ec) {
            std::cerr << "Receive error: " << ec.message() << '\n';
            return 1;
        }
        if (!hardware.isValid())
            break;
        if (!process(std::move(hardware)))
            return 1;
    }

    for (;;) {
        av::Packet output = encoder.encode(ec);
        if (ec) {
            std::cerr << "Encoder flush error: " << ec.message() << '\n';
            return 1;
        }
        if (!output)
            break;
        output.setStreamIndex(0);
        octx.writePacket(output, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << '\n';
            return 1;
        }
        ++frames;
    }

    octx.writeTrailer(ec);
    if (ec) {
        std::cerr << "Can't write trailer: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "frames: " << frames << '\n';
    return 0;
}
