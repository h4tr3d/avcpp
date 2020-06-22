#include <catch2/catch.hpp>

#include <vector>

#include "packet.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

namespace {

const uint8_t pkt_data[] = {
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

        CHECK(in_pkt.refCount() == 1);

        {
            auto pkt = in_pkt;
            CHECK(in_pkt.refCount() == 2);
            CHECK(pkt.raw() != in_pkt.raw());
            CHECK(pkt.raw()->data == in_pkt.raw()->data);
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
}

