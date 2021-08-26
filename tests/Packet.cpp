#include <catch2/catch.hpp>

#include <vector>

#include "packet.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

namespace {

constexpr uint8_t pkt_data[] = {
    1,2,3,4,5,6,7,8,9,0
};

constexpr size_t static_pkt_size = 10;
uint8_t static_pkt_data[static_pkt_size + AV_INPUT_BUFFER_PADDING_SIZE] = {
    1,2,3,4,5,6,7,8,9,0
};

}

TEST_CASE("Packet define", "[Packet][Construct]")
{
    SECTION("Construct from data") {
        av::Packet pkt{pkt_data, sizeof(pkt_data)};
        CHECK(pkt.refCount() == 1);
        CHECK(pkt.data() != pkt_data);
        CHECK(pkt.size() == sizeof(pkt_data));
        CHECK(memcmp(pkt.data(), pkt_data, pkt.size()) == 0);
    }

    SECTION("initFromAVPacket") {
        av::Packet in_pkt{pkt_data, sizeof(pkt_data)};

        in_pkt.setPts({60, {1, 1}});
        in_pkt.setDts({61, {1, 1}});

        CHECK(in_pkt.refCount() == 1);

        {
            auto pkt = in_pkt;
            CHECK(in_pkt.refCount() == 2);
            CHECK(pkt.raw() != in_pkt.raw());
            CHECK(pkt.raw()->data == in_pkt.raw()->data);
            // TBD: Properties check
            CHECK(pkt.pts() == in_pkt.pts());
            CHECK(pkt.dts() == in_pkt.dts());
        }

        CHECK(in_pkt.refCount() == 1);

        {
            auto pkt = in_pkt.clone();
            CHECK(in_pkt.refCount() == 1);
            CHECK(pkt.refCount() == 1);
            CHECK(pkt.raw() != in_pkt.raw());
            CHECK(pkt.raw()->data != in_pkt.raw()->data);
            CHECK(pkt.raw()->size == in_pkt.raw()->size);
            CHECK(memcmp(pkt.raw()->data, in_pkt.raw()->data, in_pkt.size()) == 0);
            // TBD: Properties check
            CHECK(pkt.pts() == in_pkt.pts());
            CHECK(pkt.dts() == in_pkt.dts());
        }
    }

    SECTION("Move packet") {
        av::Packet in_pkt{pkt_data, sizeof(pkt_data)};
        CHECK(in_pkt.refCount() == 1);

        auto pkt = std::move(in_pkt);
        CHECK(pkt.refCount() == 1);
        CHECK(in_pkt.refCount() == 0);
        CHECK(in_pkt.isNull() == true);
        CHECK(memcmp(pkt.data(), pkt_data, pkt.size()) == 0);
    }

    SECTION("Wrap data") {
        auto data = av::memdup<uint8_t>(pkt_data, sizeof(pkt_data));
        av::Packet pkt{data.get(), sizeof(pkt_data), av::Packet::wrap_data{}};
        auto ptr = data.release();

        CHECK(pkt.data() == ptr);
        CHECK(pkt.refCount() == 1);
    }

    SECTION("Wrap static data") {
        av::Packet pkt{static_pkt_data, static_pkt_size, av::Packet::wrap_data_static{}};
        CHECK(pkt.data() == static_pkt_data);
        CHECK(pkt.refCount() == 1);
    }

    SECTION("setPts/Dts") {
        {
            av::Packet pkt;
            pkt.setPts({1000, {10000, 1}});
            CHECK(pkt.raw()->pts == 1000);
            CHECK(pkt.timeBase() == av::Rational{10000, 1});
            pkt.setDts({1000, {1000, 1}});
            CHECK(pkt.raw()->dts == 100); // Timebase set already
            CHECK(pkt.timeBase() == av::Rational{10000, 1});
        }
        {
            av::Packet pkt;
            pkt.setDts({1000, {10000, 1}});
            CHECK(pkt.raw()->dts == 1000);
            CHECK(pkt.timeBase() == av::Rational{10000, 1});
            pkt.setPts({1000, {1000, 1}});
            CHECK(pkt.raw()->pts == 100); // Timebase set already
            CHECK(pkt.timeBase() == av::Rational{10000, 1});
        }
        {
            av::Packet pkt;
            pkt.setPts({1000, {10000, 1}});
            CHECK(pkt.raw()->pts == 1000);
            CHECK(pkt.timeBase() == av::Rational{10000, 1});
            pkt.setTimeBase({1000, 1});
            CHECK(pkt.raw()->pts == 10000);
            CHECK(pkt.timeBase() == av::Rational{1000, 1});
        }
    }

    SECTION("Timebase Copy/Clone") {
        const av::Rational tb{1000,1};
        av::Packet in_pkt{pkt_data, sizeof(pkt_data)};
        CHECK(in_pkt.timeBase() == av::Rational());
        in_pkt.setTimeBase(tb);
        CHECK(in_pkt.timeBase() == tb);

        // copy ctor
        {
            auto pkt = in_pkt;
            CHECK(pkt.timeBase() == tb);
        }

        // move ctor
        {
            auto tmp = in_pkt;
            CHECK(tmp.timeBase() == tb);
            auto pkt = std::move(tmp);
            CHECK(pkt.timeBase() == tb);
        }

        // copy operator
        {
            av::Packet pkt;
            pkt = in_pkt;
            CHECK(pkt.timeBase() == tb);
        }

        // move operator
        {
            auto tmp = in_pkt;
            CHECK(tmp.timeBase() == tb);
            av::Packet pkt;
            pkt = std::move(tmp);
            CHECK(pkt.timeBase() == tb);
        }

        // clone
        {
            auto pkt = in_pkt.clone();
            CHECK(pkt.timeBase() == tb);
        }
    }
}

