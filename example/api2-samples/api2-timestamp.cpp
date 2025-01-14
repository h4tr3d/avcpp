#include <iostream>
#include <chrono>
#include <cassert>

#include "timestamp.h"

using namespace std;
using namespace av;

int main()
{
    {
        Timestamp a = {1, {1, 1000}};
        Timestamp b = {2, {1, 3}};

        auto c = a + b;
        av::print("{} = {}\n", c, c.seconds());

        auto d = c - b;
        av::print("{} = {}\n", d, d.seconds());

        auto e = a * b;
        av::print("{} = {}\n", e, e.seconds());

        auto f = a / b;
        av::print("{} = {}\n", f, f.seconds());

        auto g = Timestamp{300, {1, 2}} / Timestamp{150, {1, 3}};
        av::print("{} = {}\n", g, g.seconds());
    }

    {
        Timestamp ts = std::chrono::microseconds(500);
        av::print("{} = {}\n", ts, ts.seconds());

        auto ts2 = ts + std::chrono::seconds(5);
        av::print("{} = {}\n", ts2, ts2.seconds());

        auto dur1 = ts2.toDuration<std::chrono::milliseconds>();
        av::print("{}\n", dur1.count());

        auto ts3 = std::chrono::seconds(5) + ts;
        av::print("{} = {}\n", ts3, ts3.seconds());
    }
}
