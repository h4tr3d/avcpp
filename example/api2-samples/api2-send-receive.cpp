// Decodes the first video stream with the avcodec send/receive API and
// demonstrates the end-of-stream contract: call flush() once, drain with
// receive() until it returns an empty result, then check isEof().
//
// Usage: api2-send-receive <input file>

#include <cstddef>
#include <iostream>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input file>\n";
        return 1;
    }

    av::init();

    std::error_code ec;

    av::FormatContext ictx;
    ictx.openInput(argv[1], ec);
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
    for (auto &&st : ictx.streams()) {
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
            vst = st;
            break;
        }
    }

    if (!vst.isValid()) {
        std::cerr << "Video stream not found\n";
        return 1;
    }

    av::VideoDecoderContext dec{vst};
    dec.open(ec);
    if (ec) {
        std::cerr << "Can't open codec: " << ec.message() << '\n';
        return 1;
    }

    std::size_t packets = 0;
    std::size_t frames = 0;

    // receive() returns an empty frame for both EAGAIN and EOF. The loop ends
    // only when the decoder has no output left to produce.
    auto drain = [&]() -> bool {
        for (;;) {
            auto frame = dec.receive(ec);
            if (ec) {
                std::cerr << "Can't receive a frame: " << ec.message() << '\n';
                return false;
            }
            if (!frame)
                return true;
            ++frames;
        }
    };

    av::Packet pkt;
    while ((pkt = ictx.readPacket(ec))) {
        if (pkt.streamIndex() != vst.index())
            continue;

        ++packets;

        for (;;) {
            bool accepted = dec.send(pkt, ec);
            if (ec) {
                std::cerr << "Can't send a packet: " << ec.message() << '\n';
                return 1;
            }

            // Drain what is available. A rejected send() means EAGAIN: the
            // packet must be retried after the output queue is empty.
            if (!drain())
                return 1;

            if (accepted)
                break;
        }
    }

    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    // Switch the decoder to draining mode. flush() is idempotent and reports
    // false only when the output queue must be drained before it is retried.
    if (!dec.flush(ec)) {
        if (ec) {
            std::cerr << "Can't flush the decoder: " << ec.message() << '\n';
            return 1;
        }
        if (!drain())
            return 1;
        if (!dec.flush(ec)) {
            std::cerr << "Can't flush the decoder after draining\n";
            return 1;
        }
    }

    if (!drain())
        return 1;

    if (!dec.isEof()) {
        std::cerr << "Decoder did not reach EOF after draining\n";
        return 1;
    }

    std::cout << "packets: " << packets << ", frames: " << frames << '\n';
    return 0;
}
