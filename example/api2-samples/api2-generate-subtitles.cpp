// Generates ASS subtitles for a video and writes them to a playable file.
//
// Every video frame produces two subtitle events: the zero-based frame number
// in the bottom-left corner and the frame timestamp in the bottom-right
// corner. ASS alignment tags ({\an1} and {\an3}) are used because SubRip and
// other text subtitle formats cannot position events in a corner.
//
// Video and audio packets are not decoded: they are copied from the input
// container to the output one. The output extension selects the muxer:
//
//   *.mkv  - copied video and audio streams plus the generated ASS subtitles;
//   *.ass  - subtitle-only file with the same events.
//
// Usage: api2-generate-subtitles <input video> <output.mkv|output.ass>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"
#include "avcpp/subtitle.h"

namespace {

std::string formatTimestamp(double seconds)
{
    auto const totalMilliseconds =
        static_cast<std::int64_t>(std::llround(seconds * 1000.0));
    auto const milliseconds = totalMilliseconds % 1000;
    auto const totalSeconds = totalMilliseconds / 1000;

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld.%03lld",
                  static_cast<long long>(totalSeconds / 3600),
                  static_cast<long long>((totalSeconds / 60) % 60),
                  static_cast<long long>(totalSeconds % 60),
                  static_cast<long long>(milliseconds));
    return buffer;
}

std::string makeAssEvent(int readOrder, std::string_view text,
                         const char *alignment)
{
    std::string event = std::to_string(readOrder);
    event += ",0,Default,,0,0,0,,";
    event += alignment;
    event += text;
    return event;
}

av::Packet encodeEvent(av::SubtitleEncoderContext &enc,
                       const std::string &assEvent,
                       int64_t pts, int durationMs,
                       av::OptionalErrorCode ec)
{
    auto frame = av::SubtitleFrame::fromAss(assEvent, ec);
    if (ec && *ec)
        return {};

    frame.setTimeBase(av::Rational(1, 100));
    frame.setPts(av::Timestamp(pts, av::Rational(1, 100)));
    frame.setStartDisplayTime(0);
    frame.setEndDisplayTime(static_cast<uint32_t>(durationMs));

    return enc.encode(frame, ec);
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input video> <output.mkv|output.ass>\n";
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

    av::Rational fps = vst.averageFrameRate();
    if (!fps.getNumerator() || !fps.getDenominator())
        fps = vst.frameRate();
    if (!fps.getNumerator() || !fps.getDenominator()) {
        std::cerr << "Can't determine the video frame rate\n";
        return 1;
    }

    double const frameDuration = static_cast<double>(fps.getDenominator())
                                 / static_cast<double>(fps.getNumerator());

    double const startTime = vst.startTime().isNoPts()
                                 ? 0.0
                                 : vst.startTime().seconds();

    auto codec = av::findEncodingCodec(AV_CODEC_ID_ASS);
    if (codec.isNull()) {
        std::cerr << "ASS encoder not found\n";
        return 1;
    }

    // The ASS encoder measures time in centiseconds, matching the ASS muxer.
    av::SubtitleEncoderContext enc{codec};
    enc.setTimeBase(av::Rational(1, 100));
    enc.setDefaultSubtitleHeader();
    enc.open(ec);
    if (ec) {
        std::cerr << "Can't open the subtitle encoder: " << ec.message() << '\n';
        return 1;
    }

    av::FormatContext octx;
    octx.openOutput(argv[2], ec);
    if (ec) {
        std::cerr << "Can't open output: " << ec.message() << '\n';
        return 1;
    }

    // The ASS muxer accepts a single subtitle stream and no media streams.
    // Matroska and similar containers get the copied media streams as well.
    char const *formatName = octx.outputFormat().name();
    bool const subtitleOnly = formatName && std::string_view(formatName) == "ass";

    std::vector<int> streamMap(ictx.streamsCount(), -1);
    if (!subtitleOnly) {
        for (auto &&ist : ictx.streams()) {
            if (ist.mediaType() != AVMEDIA_TYPE_VIDEO &&
                ist.mediaType() != AVMEDIA_TYPE_AUDIO)
                continue;

            av::Stream ost = octx.addStream(ec);
            if (ec) {
                std::cerr << "Can't add an output stream: " << ec.message() << '\n';
                return 1;
            }

            ost.setTimeBase(ist.timeBase());
            ost.setFrameRate(ist.frameRate());
            ost.setAverageFrameRate(ist.averageFrameRate());
            ost.codecParameters().copyFrom(ist.codecParameters(), ec);
            if (ec) {
                std::cerr << "Can't copy stream parameters: " << ec.message() << '\n';
                return 1;
            }

            streamMap[ist.index()] = static_cast<int>(ost.index());
        }
    }

    av::Stream sst = octx.addStream(ec);
    if (ec) {
        std::cerr << "Can't add a subtitle stream: " << ec.message() << '\n';
        return 1;
    }

    // Copies the codec type, id and the ASS header (extradata) from the
    // encoder, so the muxer can write the script header.
    sst.setTimeBase(av::Rational(1, 100));
    sst.codecParameters().copyFrom(enc, ec);
    if (ec) {
        std::cerr << "Can't copy codec parameters: " << ec.message() << '\n';
        return 1;
    }

    octx.writeHeader(ec);
    if (ec) {
        std::cerr << "Can't write the output header: " << ec.message() << '\n';
        return 1;
    }

    int const durationMs = static_cast<int>(std::llround(frameDuration * 1000.0));
    int readOrder = 0;
    std::int64_t nextFrame = 0;

    auto emitFrame = [&](std::int64_t frameIndex) {
        double const timestamp =
            startTime + static_cast<double>(frameIndex) * frameDuration;
        auto const pts = static_cast<std::int64_t>(
            std::llround(timestamp * 100.0));

        // One ASS event holds a single rectangle, so each corner gets its own
        // event with the same timing.
        auto number = encodeEvent(enc,
                                  makeAssEvent(readOrder++,
                                               "Frame " + std::to_string(frameIndex),
                                               "{\\an1}"),
                                  pts, durationMs, ec);
        if (ec) {
            std::cerr << "Can't encode the frame number: " << ec.message() << '\n';
            return false;
        }

        auto stamp = encodeEvent(enc,
                                 makeAssEvent(readOrder++,
                                              formatTimestamp(timestamp),
                                              "{\\an3}"),
                                 pts, durationMs, ec);
        if (ec) {
            std::cerr << "Can't encode the timestamp: " << ec.message() << '\n';
            return false;
        }

        number.setStreamIndex(sst.index());
        octx.writePacket(number, ec);
        if (ec) {
            std::cerr << "Can't write the frame number: " << ec.message() << '\n';
            return false;
        }

        stamp.setStreamIndex(sst.index());
        octx.writePacket(stamp, ec);
        if (ec) {
            std::cerr << "Can't write the timestamp: " << ec.message() << '\n';
            return false;
        }

        return true;
    };

    // Subtitles are emitted while the input is read, so the muxer can
    // interleave all streams instead of buffering the whole subtitle track.
    auto emitUntil = [&](double seconds) {
        while (startTime + static_cast<double>(nextFrame) * frameDuration
                   <= seconds + 1e-9) {
            if (!emitFrame(nextFrame))
                return false;
            ++nextFrame;
        }
        return true;
    };

    double latestPts = -1.0;

    av::Packet pkt;
    while ((pkt = ictx.readPacket(ec))) {
        if (pkt.streamIndex() == vst.index() && pkt.pts().isValid())
            latestPts = std::max(latestPts, pkt.pts().seconds());

        if (!subtitleOnly) {
            int const dstIndex = streamMap[pkt.streamIndex()];
            if (dstIndex >= 0) {
                av::Packet out = pkt;
                out.setStreamIndex(dstIndex);
                octx.writePacket(out, ec);
                if (ec) {
                    std::cerr << "Can't copy a media packet: " << ec.message() << '\n';
                    return 1;
                }
            }
        }

        if (latestPts >= 0.0 && !emitUntil(latestPts))
            return 1;
    }

    if (ec) {
        std::cerr << "Read error: " << ec.message() << '\n';
        return 1;
    }

    if (!emitUntil(latestPts))
        return 1;

    octx.writeTrailer(ec);
    if (ec) {
        std::cerr << "Can't write the output trailer: " << ec.message() << '\n';
        return 1;
    }

    std::cout << "Wrote " << nextFrame << " frames at "
              << fps.getNumerator() << '/' << fps.getDenominator()
              << " fps to " << argv[2] << '\n';
    return 0;
}
