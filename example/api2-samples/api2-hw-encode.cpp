// Hardware encoder sample with synthetic frames.
//
// Usage: api2-hw-encode <device> <output> [frame count]
//
// The sample configures hardware encoding in Auto mode. Software frames are
// uploaded automatically before they reach the encoder.

#include <iostream>
#include <string>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/format.h"
#include "avcpp/formatcontext.h"
#include "avcpp/frame.h"
#include "avcpp/hwcontext.h"
#include "avcpp/packet.h"

namespace {

void fillFrame(av::VideoFrame& frame, int index)
{
    AVFrame* raw = frame.raw();
    const AVPixFmtDescriptor* desc =
        av_pix_fmt_desc_get(static_cast<AVPixelFormat>(raw->format));
    if (!desc)
        return;

    const int planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(raw->format));
    for (int plane = 0; plane < planes; ++plane) {
        if (!raw->data[plane] || raw->linesize[plane] <= 0)
            continue;

        int height = raw->height;
        // The second and third planes are chroma planes in most YUV formats.
        if (plane == 1 || plane == 2)
            height >>= desc->log2_chroma_h;

        const uint8_t value = static_cast<uint8_t>((index * 3 + plane * 40) & 0xFF);
        for (int row = 0; row < height; ++row) {
            uint8_t* line = raw->data[plane] + row * raw->linesize[plane];
            for (int x = 0; x < raw->linesize[plane]; ++x)
                line[x] = value;
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <device> <output> [frame count]\n";
        return 1;
    }

    av::init();

    const av::hw::DeviceType type = av::hw::DeviceType::fromName(argv[1]);
    if (type.isNone()) {
        std::cerr << "Unknown device type: " << argv[1] << '\n';
        return 1;
    }

    const int width = 320;
    const int height = 240;
    const int frameCount = argc > 3 ? std::stoi(argv[3]) : 50;

    std::error_code ec;

    av::hw::DeviceContext device = av::hw::DeviceContext::create(type, nullptr, {}, ec);
    if (!device.isValid()) {
        std::cerr << "Can't create device: " << ec.message() << '\n';
        return 2;
    }

    av::Codec codec = av::hw::findEncoder(AV_CODEC_ID_H264, type);
    if (!codec.raw()) {
        std::cerr << "No hardware H.264 encoder for this device\n";
        return 2;
    }

    av::OutputFormat ofrmt;
    ofrmt.setFormat(std::string(), argv[2]);
    av::FormatContext octx;
    octx.setFormat(ofrmt);

    av::VideoEncoderContext encoder(codec);
    encoder.setWidth(width);
    encoder.setHeight(height);
    encoder.setTimeBase(av::Rational{1, 25});
    encoder.setFrameRate(av::Rational{25, 1});
    encoder.setBitRate(1'000'000);
    if (octx.outputFormat().isFlags(AVFMT_GLOBALHEADER))
        encoder.addFlags(AV_CODEC_FLAG_GLOBAL_HEADER);

    encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                                AV_PIX_FMT_NONE, ec);
    if (ec) {
        std::cerr << "Can't configure hardware encoding: " << ec.message() << '\n';
        return 1;
    }

    av::PixelFormat softwareFormat(AV_PIX_FMT_NV12);
    const av::hw::FramesContext frames = encoder.hwFramesContext();
    if (frames.isValid())
        softwareFormat = frames.swFormat();

    encoder.open(codec, ec);
    if (ec) {
        std::cerr << "Can't open encoder: " << ec.message() << '\n';
        return 1;
    }

    av::Stream ost = octx.addStream(encoder);
    ost.setFrameRate(encoder.frameRate());
    ost.setAverageFrameRate(encoder.frameRate());
    ost.setTimeBase(encoder.timeBase());

    octx.openOutput(argv[2], ec);
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

    std::cout << "encoder: " << codec.name()
              << ", software format: " << softwareFormat.name()
              << ", frames: " << frameCount << '\n';

    int written = 0;
    for (int index = 0; index < frameCount; ++index) {
        av::VideoFrame frame(softwareFormat, width, height, 32);
        if (!frame.isValid()) {
            std::cerr << "Can't allocate a frame\n";
            return 1;
        }
        fillFrame(frame, index);

        frame.setPts(av::Timestamp(index, encoder.timeBase()));
        frame.setTimeBase(encoder.timeBase());
        frame.setStreamIndex(0);

        av::Packet packet = encoder.encode(frame, ec);
        if (ec) {
            std::cerr << "Encode error: " << ec.message() << '\n';
            return 1;
        }
        if (!packet)
            continue;

        packet.setStreamIndex(0);
        octx.writePacket(packet, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << '\n';
            return 1;
        }
        ++written;
    }

    for (;;) {
        av::Packet packet = encoder.encode(ec);
        if (ec) {
            std::cerr << "Encoder flush error: " << ec.message() << '\n';
            return 1;
        }
        if (!packet)
            break;
        packet.setStreamIndex(0);
        octx.writePacket(packet, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << '\n';
            return 1;
        }
        ++written;
    }

    octx.writeTrailer(ec);
    if (ec) {
        std::cerr << "Can't write trailer: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "packets: " << written << '\n';
    return 0;
}
