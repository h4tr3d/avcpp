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
        cout << c << " = " << c.seconds() << endl;

        auto d = c - b;
        cout << d << " = " << d.seconds() << endl;

        auto e = a * b;
        cout << e << " = " << e.seconds() << endl;

        auto f = a / b;
        cout << f << " = " << f.seconds() << endl;

        auto g = Timestamp{300, {1, 2}} / Timestamp{150, {1, 3}};
        cout << g << " = " << g.seconds() << endl;
    }

    {
        Timestamp ts = std::chrono::microseconds(500);
        cout << ts << " = " << ts.seconds() << endl;

        auto ts2 = ts + std::chrono::seconds(5);
        cout << ts2 << " = " << ts2.seconds() << endl;

        auto dur1 = ts2.toDuration<std::chrono::milliseconds>();
        cout << dur1.count() << endl;

        auto ts3 = std::chrono::seconds(5) + ts;
        cout << ts3 << " = " << ts3.seconds() << endl;



    }
}
