// Zero-copy hardware transcode.
//
// Usage: api2-hw-transcode <device> <input> <output>
//
// The sample reuses the frames context created by the hardware decoder for the
// hardware encoder. This is the same approach as FFmpeg's vaapi_transcode
// example: the decoded surfaces are passed to the encoder without a copy.
//
// If the zero-copy path is not available, the sample transparently switches to
// the copy path: hardware decode -> system memory -> hardware upload -> encode.

#include <iostream>
#include <string>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/format.h"
#include "avcpp/formatcontext.h"
#include "avcpp/hwcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"

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

    av::OutputFormat ofrmt;
    ofrmt.setFormat(std::string(), argv[3]);
    av::FormatContext octx;
    octx.setFormat(ofrmt);

    av::VideoEncoderContext encoder(encoderCodec);
    av::Stream ost;
    bool outputReady = false;
    bool zeroCopy = false;
    int64_t index = 0;

    auto setupOutput = [&](const av::VideoFrame& frame) -> bool {
        encoder.setWidth(frame.width());
        encoder.setHeight(frame.height());
        encoder.setTimeBase(av::Rational{1, 1000});
        encoder.setFrameRate(vst.frameRate());
        encoder.setBitRate(decoder.bitRate());
        if (octx.outputFormat().isFlags(AVFMT_GLOBALHEADER))
            encoder.addFlags(AV_CODEC_FLAG_GLOBAL_HEADER);

        encoder.configureHwEncoding(frame.hwFramesContext(), ec);
        zeroCopy = !ec;

        if (!zeroCopy) {
            std::cerr << "Zero-copy path is not available: " << ec.message()
                      << "; using the copy path\n";
            ec.clear();
            encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                                        AV_PIX_FMT_NONE, ec);
        }
        if (ec) {
            std::cerr << "Can't configure hardware encoding: " << ec.message() << '\n';
            return false;
        }

        encoder.open(encoderCodec, ec);
        if (ec) {
            std::cerr << "Can't open encoder: " << ec.message() << '\n';
            return false;
        }

        ost = octx.addStream(encoder);
        ost.setFrameRate(vst.frameRate());
        ost.setAverageFrameRate(vst.frameRate());
        ost.setTimeBase(encoder.timeBase());

        octx.openOutput(argv[3], ec);
        if (ec) {
            std::cerr << "Can't open output: " << ec.message() << '\n';
            return false;
        }
        octx.writeHeader(ec);
        octx.flush();
        if (ec) {
            std::cerr << "Can't write header: " << ec.message() << '\n';
            return false;
        }

        outputReady = true;
        std::cout << "encoder: " << encoderCodec.name()
                  << ", path: " << (zeroCopy ? "zero-copy" : "copy") << '\n';
        return true;
    };

    auto encodeFrame = [&](av::VideoFrame frame) -> bool {
        if (!outputReady && !setupOutput(frame))
            return false;

        const av::Timestamp sourceTimestamp = frame.pts();

        if (!zeroCopy && frame.isHwFrame()) {
            frame = frame.toSystemMemory(AV_PIX_FMT_NONE, ec);
            if (ec) {
                std::cerr << "Can't transfer frame to system memory: "
                          << ec.message() << '\n';
                return false;
            }
        }

        frame.setTimeBase(encoder.timeBase());
        frame.setStreamIndex(0);
        if (sourceTimestamp.isValid() && !sourceTimestamp.isNoPts())
            frame.setPts(sourceTimestamp);
        else
            frame.setPts(av::Timestamp(index, encoder.timeBase()));
        ++index;

        av::Packet packet = encoder.encode(frame, ec);
        if (ec) {
            std::cerr << "Encode error: " << ec.message() << '\n';
            return false;
        }
        if (!packet)
            return true;

        packet.setStreamIndex(0);
        octx.writePacket(packet, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << '\n';
            return false;
        }
        return true;
    };

    auto drainDecoder = [&]() -> bool {
        for (;;) {
            av::VideoFrame frame = decoder.receive(ec);
            if (ec) {
                std::cerr << "Receive error: " << ec.message() << '\n';
                return false;
            }
            if (!frame)
                return true;
            if (!encodeFrame(std::move(frame)))
                return false;
        }
    };

    std::size_t packets = 0;
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
            if (!drainDecoder())
                return 1;
            if (accepted)
                break;
        }
    }
    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    if (!decoder.flush(ec)) {
        if (ec) {
            std::cerr << "Flush error: " << ec.message() << '\n';
            return 1;
        }
        if (!drainDecoder())
            return 1;
        if (!decoder.flush(ec)) {
            std::cerr << "Can't flush decoder\n";
            return 1;
        }
    }
    if (!drainDecoder())
        return 1;

    if (outputReady) {
        for (;;) {
            av::Packet flushed = encoder.encode(ec);
            if (ec) {
                std::cerr << "Encoder flush error: " << ec.message() << '\n';
                return 1;
            }
            if (!flushed)
                break;
            flushed.setStreamIndex(0);
            octx.writePacket(flushed, ec);
            if (ec) {
                std::cerr << "Write error: " << ec.message() << '\n';
                return 1;
            }
        }

        octx.writeTrailer(ec);
        if (ec) {
            std::cerr << "Can't write trailer: " << ec.message() << '\n';
            return 1;
        }
    }

    std::cout << "packets: " << packets << '\n';
    return 0;
}
