#include "timestamp.h"

namespace av {

Timestamp::Timestamp() noexcept
{
}

Timestamp::Timestamp(int64_t timestamp, const Rational &timebase) noexcept
    : m_timestamp(timestamp),
      m_timebase(timebase)
{

}

int64_t Timestamp::timestamp() const noexcept
{
    return m_timestamp;
}

const Rational &Timestamp::timebase() const noexcept
{
    return m_timebase;
}

bool Timestamp::isValid() const noexcept
{
    return m_timestamp != AV_NOPTS_VALUE;
}

Timestamp::operator double() const noexcept
{
    return seconds();
}

double Timestamp::seconds() const noexcept
{
    return m_timebase.getDouble() * m_timestamp;
}

Timestamp::operator bool() const noexcept
{
    return m_timestamp != AV_NOPTS_VALUE;
}

Timestamp operator+(const Timestamp &left, const Timestamp &right) noexcept
{
    // Use more good precision
    if (left.timebase() < right.timebase()) {
        auto ts = av_add_stable(left.timebase(), left.timestamp(),
                                right.timebase(), right.timestamp());
        return {ts, left.timebase()};
    } else {
        auto ts = av_add_stable(right.timebase(), right.timestamp(),
                                left.timebase(), left.timestamp());
        return {ts, right.timebase()};
    }
}

Timestamp operator-(const Timestamp &left, const Timestamp &right) noexcept
{
    // Use more good precision
    auto tb = std::min(left.timebase(), right.timebase());
    auto tsleft  = left.timebase().rescale(left.timestamp(), tb);
    auto tsright = right.timebase().rescale(right.timestamp(), tb);

    auto ts = tsleft - tsright;

    return {ts, tb};
}

Timestamp operator*(const Timestamp &left, const Timestamp &right) noexcept
{
    auto ts = left.timestamp() * right.timestamp();
    auto tb = left.timebase() * right.timebase();
    return {ts, tb};
}

Timestamp operator/(const Timestamp &left, const Timestamp &right) noexcept
{
    int num, den;

    // Use more good precision
    auto tb = std::min(left.timebase(), right.timebase());
    auto tsleft  = left.timebase().rescale(left.timestamp(), tb);
    auto tsright = right.timebase().rescale(right.timestamp(), tb);

    int64_t ts = 1;
    if (tsleft > tsright) {
        ts = tsleft / tsright;
        tsleft %= tsright;
        if (tsleft == 0) {
            tsleft = 1;
            tsright = 1;
        }
    }

    av_reduce(&num, &den, tsleft, tsright, INT64_MAX);

    return {ts, {num, den}};
}

} // ::av
