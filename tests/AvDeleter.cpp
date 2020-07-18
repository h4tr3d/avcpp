#include <catch2/catch.hpp>

#include <vector>

#include "avutils.h"
#include "frame.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

TEST_CASE("Deleter Checking", "[AvUniquePtrDeleter]")
{
#if 0
    // use AvDeleter with unique_ptr can lead to build error:
    using deleter_t = av::AvDeleter;
#else
    using deleter_t = av::AvUniquePtrDeleter;
#endif
    std::unique_ptr<AVPacket, deleter_t> pkt(av_packet_alloc(), deleter_t{});
}

TEST_CASE("Deleter Checking", "[AvDeleter]")
{
    SECTION("AVFrame") {
        av::AvDeleter deleter;

        AVFrame *frame = av_frame_alloc();
        CHECK(frame);

        frame->width = 1920;
        frame->height = 1080;
        frame->format = AV_PIX_FMT_RGB24;

        auto sts = av_frame_get_buffer(frame, 0);
        CHECK(sts == 0);
        CHECK(frame->buf[0]);
        CHECK(frame->data[0]);
        CHECK(frame->buf[0]->data == frame->data[0]);

        auto buf_ref = frame->buf[0];
        CHECK(buf_ref->size >= 1920*1080*3);
        CHECK(av_buffer_get_ref_count(buf_ref) == 1);

        // store
        auto buf_ref_copy = *buf_ref;

        {
            auto clone = av_frame_clone(frame);
            CHECK(av_buffer_get_ref_count(buf_ref) == 2);
            deleter(clone);
            CHECK(av_buffer_get_ref_count(buf_ref) == 1);
        }

        deleter(frame);
        CHECK(frame == nullptr);
    }

    SECTION("AVPacket") {
        av::AvDeleter deleter;

        AVPacket *pkt = av_packet_alloc();
        CHECK(pkt);

        int sts = av_new_packet(pkt, 31337);
        CHECK(sts == 0);
        CHECK(pkt->buf);
        CHECK(pkt->data);
        CHECK(pkt->size == 31337);
        CHECK(pkt->buf->data == pkt->data);

        auto buf = pkt->buf;

        {
            auto clone = av_packet_clone(pkt);
            CHECK(av_buffer_get_ref_count(buf) == 2);
            deleter(clone);
            CHECK(av_buffer_get_ref_count(buf) == 1);
            CHECK(clone == nullptr);
        }

        deleter(pkt);
        CHECK(pkt == nullptr);
    }

}
