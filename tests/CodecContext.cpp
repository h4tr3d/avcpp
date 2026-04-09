//
//
// TODO: test incomplete. Sounds like RAWVIDEO decoder does not support multiple frames per-packet: only first one
//       consumed
//
//

#include <catch2/catch_test_macros.hpp>

#include "avcpp/avconfig.h"

#include <vector>

#include "avcpp/packet.h"
#include "avcpp/codeccontext.h"

namespace {
constexpr std::size_t W = 320;
constexpr std::size_t H = 240;
const     av::PixelFormat FMT{AV_PIX_FMT_GRAY8};
constexpr std::size_t FRAMES = 4;
constexpr std::size_t FRAME_SIZE = W * H;
constexpr std::size_t FRAMES_SIZE = FRAME_SIZE * FRAMES;
}

TEST_CASE("CodecContext")
{
    SECTION("Decode packet with multiple frames") {

        std::vector<uint8_t> block;
        block.reserve(FRAMES_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
        block.resize(FRAMES_SIZE); // Gray

        av::Codec vcodec = av::findDecodingCodec(AVCodecID::AV_CODEC_ID_RAWVIDEO);
        av::VideoDecoderContext vdec{vcodec};
        vdec.setHeight(H);
        vdec.setWidth(W);
        vdec.setPixelFormat(FMT);
        vdec.setTimeBase({1, 1});
        vdec.open();

        for (auto i = 0; i < FRAMES; ++i) {
            std::fill_n(block.data() + i * FRAME_SIZE, FRAME_SIZE, (i + 1) * 50);
        }

        //
        av::Packet pkt{block, av::Packet::wrap_data_static{}};
        pkt.setTimeBase(vdec.timeBase());

        static std::size_t ITERS = 4;
        std::size_t framesCount = 0;
        for (auto i = 0; i < ITERS; ++i) {
            pkt.setPts({i, {1,1}});
            auto frame = vdec.decode(pkt);
            if (!frame) // TBD
                break;
            // in case, when multiple frames per packet
            while (frame) {
                std::cout << "Frame: " << int(frame.data()[0]) << ", cnt=" << (framesCount++) << std::endl;
                frame = vdec.decode(av::Packet{nullptr});
            }
        }

        // TBD: rawvideo does not support multiple frames per packet.
        // CHECK(framesCount == ITERS * FRAMES);
    }
}