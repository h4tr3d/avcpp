// Decodes the first subtitle stream and prints the decoded subtitle frames.
//
// Subtitle codecs use the legacy avcodec_decode_subtitle2() API, so
// SubtitleDecoderContext::decode() is called once per packet instead of the
// send/receive pair.
//
// Usage: api2-subtitles <input file>

#include <cstddef>
#include <iostream>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"
#include "avcpp/subtitle.h"

namespace {

void printRect(const av::SubtitleRect &rect, std::size_t index)
{
    std::cout << "  rect " << index << ": ";

    switch (rect.type()) {
    case SUBTITLE_TEXT:
        std::cout << "text: " << rect.text();
        break;
    case SUBTITLE_ASS:
        std::cout << "ass: " << rect.ass();
        break;
    case SUBTITLE_BITMAP:
        std::cout << "bitmap: " << rect.width() << 'x' << rect.height()
                  << ", colors: " << rect.nbColors()
                  << ", x: " << rect.x() << ", y: " << rect.y();
        break;
    default:
        std::cout << "type: " << static_cast<int>(rect.type());
        break;
    }

    if (rect.isForced())
        std::cout << " (forced)";

    std::cout << '\n';
}

} // namespace

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

    av::Stream sst;
    for (auto &&st : ictx.streams()) {
        if (st.mediaType() == AVMEDIA_TYPE_SUBTITLE) {
            sst = st;
            break;
        }
    }

    if (!sst.isValid()) {
        std::cerr << "Subtitle stream not found\n";
        return 1;
    }

    av::SubtitleDecoderContext dec{sst};
    dec.open(ec);
    if (ec) {
        std::cerr << "Can't open codec: " << ec.message() << '\n';
        return 1;
    }

    std::size_t packets = 0;
    std::size_t frames = 0;

    av::Packet pkt;
    while ((pkt = ictx.readPacket(ec))) {
        if (pkt.streamIndex() != sst.index())
            continue;

        ++packets;

        std::size_t decodedBytes = 0;
        auto frame = dec.decode(pkt, 0, decodedBytes, ec);
        if (ec) {
            std::cerr << "Can't decode a subtitle: " << ec.message() << '\n';
            return 1;
        }

        // A packet can be consumed without producing a complete subtitle.
        if (!frame)
            continue;

        ++frames;

        std::cout << "subtitle " << frames
                  << ": pts " << frame.pts()
                  << " (" << frame.pts().seconds() << " s)"
                  << ", display " << frame.startDisplayTime()
                  << ".." << frame.endDisplayTime() << " ms"
                  << ", rects: " << frame.numRects() << '\n';

        for (std::size_t i = 0; i < frame.numRects(); ++i)
            printRect(frame.rect(i), i);
    }

    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "packets: " << packets << ", subtitles: " << frames << '\n';
    return 0;
}
