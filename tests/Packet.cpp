#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "avcpp/avconfig.h"
#include "avcpp/packet.h"

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

#if AVCPP_CXX_STANDARD >= 20
    SECTION("Construct from data: span") {
        std::span s = pkt_data;
        av::Packet pkt{s};
        CHECK(pkt.refCount() == 1);
        CHECK(pkt.data() != pkt_data);
        CHECK(pkt.size() == sizeof(pkt_data));
        CHECK(memcmp(pkt.data(), pkt_data, pkt.size()) == 0);
    }
#endif

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

#if AVCPP_HAS_PKT_SIDE_DATA
    SECTION("Packet side data")
    {
        const av::Rational tb{1000,1};
        av::Packet in_pkt{pkt_data, sizeof(pkt_data)};
        CHECK(in_pkt.timeBase() == av::Rational());
        in_pkt.setTimeBase(tb);
        CHECK(in_pkt.timeBase() == tb);

        {
            auto sd = in_pkt.sideData(AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL);
            REQUIRE(sd.empty());
        }

        // Iterate on null packet
        {
            av::Packet pkt;
            // its a hack to gen null for the nested m_raw
            //auto ptr = frame.raw();
            pkt.reset();
            REQUIRE(pkt.raw() == nullptr);

            int count{};
            for (auto&& sd : pkt.sideData()) {
                ++count;
            }
            REQUIRE(count == 0);
        }

        std::string meta = "field=val;field2=val2";

        {
            std::vector<uint8_t> side_block_adds(meta.size() + sizeof(uint64_t));
            std::span out = side_block_adds;
            uint64_t id = 100;
            if (std::endian::native == std::endian::little)
                id = av_bswap64(id);
            memcpy(out.data(), &id, sizeof(id));
            out = out.subspan(sizeof(id));
            std::ranges::copy(meta, out.begin());

            REQUIRE_NOTHROW(in_pkt.addSideData(AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL, side_block_adds));
            REQUIRE(in_pkt.sideDataCount() == 1);
        }

        {
            auto sd = in_pkt.sideData(AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL);
            REQUIRE(!sd.empty());
            REQUIRE(sd.size() == sizeof(uint64_t) + meta.size());

            auto in = sd;
            uint64_t id{};
            memcpy(&id, in.data(), sizeof(uint64_t));
            in = in.subspan(sizeof(uint64_t));
            if (std::endian::native == std::endian::little)
                id = av_bswap64(id);
            REQUIRE(id == 100);

            std::string_view meta_read{reinterpret_cast<const char*>(in.data()), in.size()};
            REQUIRE(meta_read == meta);
        }

        {
            auto const& pkt = in_pkt;
            auto sd = pkt.sideData(AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL);
            REQUIRE(!sd.empty());
            REQUIRE(sd.size() == sizeof(uint64_t) + meta.size());

            auto in = sd;
            uint64_t id{};
            memcpy(&id, in.data(), sizeof(uint64_t));
            in = in.subspan(sizeof(uint64_t));
            if (std::endian::native == std::endian::little)
                id = av_bswap64(id);
            REQUIRE(id == 100);

            std::string_view meta_read{reinterpret_cast<const char*>(in.data()), in.size()};
            REQUIRE(meta_read == meta);
        }

    }
#endif

#ifdef __cpp_lib_print
    SECTION("std::format formatter :: Side Data")
    {
        AVPacketSideData sideDataRaw {
            .data = nullptr,
            .size = 0,
            .type = AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL,
        };

        // wrap to view
        av::PacketSideData sideData{sideDataRaw};

        {
            auto str = std::format("{}", sideData);
            REQUIRE(str == "Matroska BlockAdditional");
        }

        {
            // Long Name formatter unsupported for Side Data
            //auto str = std::format("{:l}", sideData);
            //REQUIRE(str == "Matroska BlockAdditional");
        }

    }
#endif
}

