#include <catch2/catch.hpp>

#include <vector>

#include "frame.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

namespace {

vector<uint8_t> rgb24_raw_data;
vector<uint8_t> i420_raw_data;
vector<uint8_t> nv12_raw_data;

constexpr av::PixelFormat rgb24_pixfmt{AV_PIX_FMT_RGB24};
constexpr av::PixelFormat i420_pixfmt{AV_PIX_FMT_YUV420P};
constexpr av::PixelFormat nv12_pixfmt{AV_PIX_FMT_NV12};

constexpr int width = 640;
constexpr int height = 480;
constexpr int alignment = 1;

}

TEST_CASE("Core functionality", "[Frame]")
{
    SECTION("setPts/Dts") {
        {
            av::VideoFrame frame;
            frame.setPts({1000, {10000, 1}});
            CHECK(frame.raw()->pts == 1000);
            CHECK(frame.timeBase() == av::Rational{10000, 1});
        }
        {
            av::VideoFrame frame;
            frame.setPts({1000, {10000, 1}});
            CHECK(frame.raw()->pts == 1000);
            CHECK(frame.timeBase() == av::Rational{10000, 1});
            frame.setTimeBase({1000, 1});
            CHECK(frame.raw()->pts == 10000);
            CHECK(frame.timeBase() == av::Rational{1000, 1});
        }
    }

}

TEST_CASE("Copy from raw data storage", "[VideoFrame][VideoFrameContruct]")
{
    SECTION("Packed RGB24 :: 1 plane") {
        // Prepare image
        auto &raw_data = rgb24_raw_data;
        auto pixfmt = rgb24_pixfmt;

        raw_data.resize(av_image_get_buffer_size(pixfmt, width, height, alignment));
        
        for (size_t i = 0; i < raw_data.size(); ++i) {
            rgb24_raw_data[i] = (i%3)+1;
        }

        av::VideoFrame frame;

        //REQUIRE_NOTHROW([&frame]() mutable {
        //    frame = av::VideoFrame{rgb24_raw_data.data(), rgb24_raw_data.size(), rgb24_pixfmt, width, height, alignment};
        //});

        frame = std::move(av::VideoFrame{raw_data.data(), raw_data.size(), pixfmt, width, height, alignment});

        CHECK(frame.raw());

        CHECK(frame.raw()->data[0] == frame.data(0));
        CHECK(frame.raw()->data[1] == frame.data(1));
        CHECK(frame.raw()->data[2] == frame.data(2));
        CHECK(frame.raw()->data[3] == frame.data(3));

        CHECK(frame.data(0));
        CHECK(frame.data(1) == nullptr);

        CHECK(frame.data(0)[0] == 1);
        CHECK(frame.data(0)[1] == 2);
        CHECK(frame.data(0)[2] == 3);

        // Frame can use internal alignments and paddings
        CHECK(frame.size() >= raw_data.size());

        // Buffer size get actual bytes count to hold image relative needed alignment
        CHECK(frame.bufferSize(alignment) == raw_data.size());

        vector<uint8_t> out;
        error_code ec;
        auto status = frame.copyToBuffer(out, alignment, ec);
        CHECK(!status == !!ec); // status must be correspond to error holding
        CHECK(status);
        CHECK(!ec);

        REQUIRE(equal(raw_data.begin(), raw_data.end(), out.begin()));

        //frame.dump();

        std::cout << frame.raw()->linesize[0]
                << ", " << frame.raw()->linesize[1]
                << ", " << frame.raw()->linesize[2]
                << endl;

        // Cases below can fail and can pass on different platforms:
        //REQUIRE(std::equal(begin(i420_raw_data), end(i420_raw_data), frame.raw()->data[0]));
    }

    SECTION("Planar I420 :: 3 planes") {
        // Prepare image
        auto &raw_data = i420_raw_data;
        auto pixfmt = i420_pixfmt;

        raw_data.resize(av_image_get_buffer_size(pixfmt, width, height, alignment));

        const uint8_t * const plane_y = raw_data.data();
        const uint8_t * const plane_u = plane_y + height*width;
        const uint8_t * const plane_v = plane_u + height*width/4;
        const uint8_t * const plane_end = plane_v + height*width/4;
        
        auto ptr = raw_data.data();
        // Y plane
        for (size_t i = 0; i < width*height; ++i) {
            *ptr++ = 1;
        }
        
        // U plane
        for (size_t i = 0; i < width*height/4; ++i) {
            *ptr++ = 2;
        }

        // V plane
        for (size_t i = 0; i < width*height/4; ++i) {
            *ptr++ = 3;
        }

        CHECK(plane_end == ptr);
        CHECK(plane_end == raw_data.data() + raw_data.size());
        CHECK(ptr - 1 == &raw_data.back());

        av::VideoFrame frame;

        //REQUIRE_NOTHROW([&frame]() mutable {
        //    frame = av::VideoFrame{rgb24_raw_data.data(), rgb24_raw_data.size(), rgb24_pixfmt, width, height, alignment};
        //});

        frame = std::move(av::VideoFrame{raw_data.data(), raw_data.size(), pixfmt, width, height, alignment});

        CHECK(frame.raw());

        CHECK(frame.raw()->data[0] == frame.data(0));
        CHECK(frame.raw()->data[1] == frame.data(1));
        CHECK(frame.raw()->data[2] == frame.data(2));
        CHECK(frame.raw()->data[3] == frame.data(3));

        CHECK(frame.data(0));
        CHECK(frame.data(1));
        CHECK(frame.data(2));
        CHECK(frame.data(3) == nullptr);

        CHECK(*frame.data(0) == 1);
        CHECK(*frame.data(1) == 2);
        CHECK(*frame.data(2) == 3);

        // Frame can use internal alignments and paddings
        CHECK(frame.size() >= raw_data.size());

        // Buffer size get actual bytes count to hold image relative needed alignment
        CHECK(frame.bufferSize(alignment) == raw_data.size());

        vector<uint8_t> out;
        error_code ec;
        auto status = frame.copyToBuffer(out, alignment, ec);
        CHECK(!status == !!ec); // status must be correspond to error holding
        CHECK(status);
        CHECK(!ec);

        REQUIRE(equal(raw_data.begin(), raw_data.end(), out.begin()));

        //frame.dump();

        std::cout << frame.raw()->linesize[0]
                << ", " << frame.raw()->linesize[1]
                << ", " << frame.raw()->linesize[2]
                << endl;

        // Cases below can fail and can pass on different platforms:
        //CHECK(frame.raw()->data[1] - frame.raw()->data[0] == width*height);
        //REQUIRE(std::equal(begin(i420_raw_data), end(i420_raw_data), frame.raw()->data[0]));
    }

    SECTION("Semi-Planar NV12 :: 2 planes") {
        // Prepare image
        auto &raw_data = nv12_raw_data;
        auto pixfmt = nv12_pixfmt;

        raw_data.resize(av_image_get_buffer_size(pixfmt, width, height, alignment));

        const uint8_t * const plane_y = raw_data.data();
        const uint8_t * const plane_uv = plane_y + height*width;
        const uint8_t * const plane_end = plane_uv + height*width/2;

        auto ptr = raw_data.data();
        // Y plane
        for (size_t i = 0; i < width*height; ++i) {
            *ptr++ = 1;
        }

        // UV plane, yes, we divide on 4 to fill two bytes at once
        for (size_t i = 0; i < width*height/4; ++i) {
            *ptr++ = 2;
            *ptr++ = 3;
        }

        CHECK(plane_end == ptr);
        CHECK(plane_end == raw_data.data() + raw_data.size());
        CHECK(ptr - 1 == &raw_data.back());

        av::VideoFrame frame;

        //REQUIRE_NOTHROW([&frame]() mutable {
        //    frame = av::VideoFrame{rgb24_raw_data.data(), rgb24_raw_data.size(), rgb24_pixfmt, width, height, alignment};
        //});

        frame = std::move(av::VideoFrame{raw_data.data(), raw_data.size(), pixfmt, width, height, alignment});

        CHECK(frame.raw());

        CHECK(frame.raw()->data[0] == frame.data(0));
        CHECK(frame.raw()->data[1] == frame.data(1));
        CHECK(frame.raw()->data[2] == frame.data(2));
        CHECK(frame.raw()->data[3] == frame.data(3));

        CHECK(frame.data(0));
        CHECK(frame.data(1));
        CHECK(frame.data(2) == nullptr);

        CHECK(*frame.data(0) == 1);
        CHECK(*(frame.data(1) + 0) == 2);
        CHECK(*(frame.data(1) + 1) == 3);

        // Frame can use internal alignments and paddings
        CHECK(frame.size() >= raw_data.size());

        // Buffer size get actual bytes count to hold image relative needed alignment
        CHECK(frame.bufferSize(alignment) == raw_data.size());

        vector<uint8_t> out;
        error_code ec;
        auto status = frame.copyToBuffer(out, alignment, ec);
        CHECK(!status == !!ec); // status must be correspond to error holding
        CHECK(status);
        CHECK(!ec);

        REQUIRE(equal(raw_data.begin(), raw_data.end(), out.begin()));

        //frame.dump();

        std::cout << frame.raw()->linesize[0]
                << ", " << frame.raw()->linesize[1]
                << ", " << frame.raw()->linesize[2]
                << endl;

        // Cases below can fail and can pass on different platforms:
        //CHECK(frame.raw()->data[1] - frame.raw()->data[0] == width*height);
        //REQUIRE(std::equal(begin(i420_raw_data), end(i420_raw_data), frame.raw()->data[0]));
    }

    SECTION("AVFrame :: Copy/Destroy") {
        using namespace av;
        VideoFrame frame{PixelFormat(AV_PIX_FMT_RGB24), 1920, 1080};

        auto raw = frame.raw();
        CHECK(raw->buf[0]);
        CHECK(raw->data[0]);
        CHECK(raw->buf[0]->data == raw->data[0]);

        auto buf_ref = raw->buf[0];
        CHECK(buf_ref->size >= 1920*1080*3);
        CHECK(av_buffer_get_ref_count(buf_ref) == 1);

        // store
        auto buf_ref_copy = *buf_ref;

        {
            auto clone = frame;
            CHECK(av_buffer_get_ref_count(&buf_ref_copy) == 2);
        }

        // Scope exit, clone must reset
        CHECK(av_buffer_get_ref_count(&buf_ref_copy) == 1);
    }
}
