#include <iostream>

#include "rational.h"

using namespace std;

namespace av
{

Rational::Rational() noexcept
    : m_value{0,0}
{
}

Rational::Rational(int numerator, int denominator) noexcept
    : m_value{numerator, denominator}
{
}

Rational::Rational(const AVRational &value) noexcept
{
    m_value = value;
}

Rational::Rational(double value, int maxPrecision) noexcept
{
    m_value = av_d2q(value, maxPrecision);
}

Rational Rational::fromDouble(double value, int maxPrecision) noexcept
{
    Rational result(av_d2q(value, maxPrecision));
    return result;
}

int64_t Rational::rescale(int64_t srcValue, const Rational &dstBase) const noexcept
{
    return av_rescale_q(srcValue, m_value, dstBase.getValue());
}

void Rational::dump() const noexcept
{
    av::print("{}/{}\n", m_value.num, m_value.den);
}

Rational &Rational::operator =(const AVRational &value) noexcept
{
    m_value = value;
    return *this;
}

Rational &Rational::operator =(double value) noexcept
{
    m_value = av_d2q(value, RationalMaxPrecision);
    return *this;
}

Rational Rational::operator +(const Rational &value) const noexcept
{
    Rational result = av_add_q(m_value, value.getValue());
    return result;
}

Rational Rational::operator -(const Rational &value) const noexcept
{
    Rational result = av_sub_q(m_value, value.getValue());
    return result;
}

Rational Rational::operator *(const Rational &value) const noexcept
{
    Rational result = av_mul_q(m_value, value.getValue());
    return result;
}

Rational Rational::operator /(const Rational &value) const noexcept
{
    Rational result = av_div_q(m_value, value.getValue());
    return result;
}

double Rational::operator ()() const noexcept
{
    return av_q2d(m_value);
}

int Rational::threewaycmp(const Rational &other) const noexcept
{
    // HACK: in this case av_cmp_q() return INT_MIN;
    if (m_value.den == 0 &&
        m_value.num == 0 &&
        other.getDenominator() == 0 &&
        other.getNumerator()   == 0)
    {
        return 0;
    }
    auto const result = av_cmp_q(m_value, other.getValue());
    return result == 0 ? 0 : result == -1 ? -1 : 1;
}

bool Rational::operator ==(const Rational &other) const noexcept
{
    return threewaycmp(other) == 0;
}

} // ::av
