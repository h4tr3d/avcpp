#include <catch2/catch.hpp>

#include <vector>

#include "rational.h"

#ifdef _MSC_VER
# pragma warning (disable : 4702) // Disable warning: unreachable code
#endif


using namespace std;

TEST_CASE("Rational Define", "[Rational][Construct]")
{
    SECTION("Construct") {
        {
            av::Rational r;
            CHECK(r.getDenominator() == 0);
            CHECK(r.getNumerator() == 0);
        }

        {
            av::Rational r{1,2};
            CHECK(r.getNumerator() == 1);
            CHECK(r.getDenominator() == 2);
        }

        {
            av::Rational r0;
            av::Rational r1{1,2};

            r0 = r1;
            CHECK(r0.getNumerator() == 1);
            CHECK(r0.getDenominator() == 2);
            CHECK(r1.getNumerator() == 1);
            CHECK(r1.getDenominator() == 2);
        }

        {
            av::Rational r0;
            av::Rational r1{1,2};

            r0 = std::move(r1);

            CHECK(r0.getNumerator() == 1);
            CHECK(r0.getDenominator() == 2);
            // r1 state unknown
            //CHECK(r1.getNumerator() == 0);
            //CHECK(r1.getDenominator() == 0);
        }
    }

    SECTION("Relation") {
        {
            av::Rational r0, r1;
            CHECK(r0 == r1);
        }
        {
            av::Rational r0{1,2}, r1{1,2};
            CHECK(r0 == r1);
            CHECK(!(r0 != r1));
            CHECK(r0 >= r1);
            CHECK(r0 <= r1);
        }
        {
            av::Rational r0{1,2}, r1{1,3};

            CHECK(r0 > r1);
            CHECK(r1 < r0);

            CHECK(r0 >= r1);
            CHECK(r1 <= r0);

            CHECK(r0 != r1);
        }
    }
}
