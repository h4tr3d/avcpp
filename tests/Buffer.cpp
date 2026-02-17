#include <catch2/catch_test_macros.hpp>
#include <numeric>

#include "avcpp/avcompat.h"
#include "avcpp/ffmpeg.h"
#include "avcpp/buffer.h"

using namespace av;

TEST_CASE("BufferRef", "[BufferRef]")
{
    static constexpr std::size_t DefaultSize = 31337;

    SECTION("ctor")
    {
        {
            BufferRef ref;
            REQUIRE(ref.isWritable() == false);
            REQUIRE(ref.refCount() == 0);
            REQUIRE(ref.size() == 0);
            REQUIRE(ref.isNull() == true);
            REQUIRE(ref.data() == nullptr);
        }

        {
            BufferRef ref{DefaultSize};
            REQUIRE(ref.isWritable() == true);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == DefaultSize);
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.data() != nullptr);
        }

        {
            BufferRef ref{DefaultSize, false};
            REQUIRE(ref.isWritable() == true);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == DefaultSize);
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.data() != nullptr);
            auto const sum = std::accumulate(ref.span().begin(), ref.span().end(), std::size_t(0));
            REQUIRE(sum == 0);
        }

        {
            uint8_t buf[1024]; // kept un-init
            BufferRef ref{buf, sizeof(buf), av::buffer::null_deleter, nullptr, 0};

            REQUIRE(ref.isWritable() == true);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == sizeof(buf));
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.constData() == buf);

            buf[sizeof(buf)/2] = 67;
            REQUIRE(ref.span()[sizeof(buf)/2] == 67);

            ref.makeWritable();
            REQUIRE(ref.isWritable() == true);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == sizeof(buf));
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.data() == buf); // should be still same buffer
        }

        {
            const uint8_t buf[1024]{}; // kept un-init
            BufferRef ref{buf, sizeof(buf), av::buffer::null_deleter, nullptr, 0};

            REQUIRE(ref.isWritable() == false);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == sizeof(buf));
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.constData() == buf);

            ref.makeWritable();
            REQUIRE(ref.isWritable() == true);
            REQUIRE(ref.refCount() == 1);
            REQUIRE(ref.size() == sizeof(buf));
            REQUIRE(ref.isNull() == false);
            REQUIRE(ref.data() != buf);
        }

        // copy ctor
        {
            BufferRef ref1{DefaultSize};
            BufferRef ref2{ref1};

            REQUIRE(ref1.isWritable() == false);
            REQUIRE(ref2.isWritable() == false);
            REQUIRE(ref1.constData() == ref2.constData());
            REQUIRE(ref1.refCount() == 2);
            REQUIRE(ref2.refCount() == 2);

            ref2.reset();
            REQUIRE(ref2.isNull());
            REQUIRE(ref2.refCount() == 0);
            REQUIRE(ref1.refCount() == 1);
            REQUIRE(ref1.isWritable() == true);

            ref1.reset();
            REQUIRE(ref1.isNull());
            REQUIRE(ref1.refCount() == 0);
        }

        // move ctor
        {
            BufferRef ref1{DefaultSize};
            BufferRef ref2{std::move(ref1)};

            REQUIRE(ref1.data() == nullptr);
            REQUIRE(ref1.refCount() == 0);

            REQUIRE(ref2.data() != nullptr);
            REQUIRE(ref2.refCount() == 1);
        }
    }

    SECTION("copy/move assign")
    {
        // copy assign
        {
            BufferRef ref1{DefaultSize};
            BufferRef ref2;

            REQUIRE(ref2.isNull());
            ref2 = ref1;

            REQUIRE(!ref2.isNull());

            REQUIRE(ref1.isWritable() == false);
            REQUIRE(ref2.isWritable() == false);
            REQUIRE(ref1.constData() == ref2.constData());
            REQUIRE(ref1.refCount() == 2);
            REQUIRE(ref2.refCount() == 2);

            ref2.reset();
            REQUIRE(ref2.isNull());
            REQUIRE(ref2.refCount() == 0);
            REQUIRE(ref1.refCount() == 1);
            REQUIRE(ref1.isWritable());

            ref1.reset();
            REQUIRE(ref1.isNull());
            REQUIRE(ref1.refCount() == 0);
        }

        // move assign
        {
            BufferRef ref1{DefaultSize};
            BufferRef ref2;

            REQUIRE(ref2.isNull());

            ref2 = std::move(ref1);
            REQUIRE(!ref2.isNull());
            REQUIRE(ref1.isNull());

            REQUIRE(ref1.data() == nullptr);
            REQUIRE(ref1.refCount() == 0);

            REQUIRE(ref2.data() != nullptr);
            REQUIRE(ref2.refCount() == 1);
        }
    }

    SECTION("clone")
    {
        // clone
        {
            BufferRef ref1{DefaultSize};
            BufferRef ref2 = ref1.clone();

            REQUIRE(ref1.data() != nullptr);
            REQUIRE(ref1.refCount() == 1);

            REQUIRE(ref2.data() != nullptr);
            REQUIRE(ref2.refCount() == 1);

            REQUIRE(ref1.constData() != ref2.constData());
            REQUIRE(ref1.size() == ref2.size());
        }
    }

    SECTION("exception on READONLY access")
    {
        const uint8_t buf[1024]{}; // kept un-init
        BufferRef ref{buf, sizeof(buf), av::buffer::null_deleter, nullptr, 0};

        REQUIRE(ref.isWritable() == false);
        REQUIRE(ref.refCount() == 1);
        REQUIRE(ref.size() == sizeof(buf));
        REQUIRE(ref.isNull() == false);
        REQUIRE_THROWS(ref.data() == buf);

        ref.makeWritable();
        REQUIRE(ref.isWritable() == true);
        REQUIRE(ref.refCount() == 1);
        REQUIRE(ref.size() == sizeof(buf));
        REQUIRE(ref.isNull() == false);
        REQUIRE_NOTHROW(ref.data() != buf);
    }
}
