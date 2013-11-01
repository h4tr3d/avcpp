#include <iostream>

#include "rational.h"

using namespace std;

namespace av
{

Rational::Rational()
{
    value.den = 0;
    value.num = 0;
}

Rational::Rational(int numerator, int denominator)
{
    value.den = denominator;
    value.num = numerator;
}

Rational::Rational(const AVRational &value)
{
    this->value = value;
}

Rational::Rational(double value, int maxPrecision)
{
    this->value = av_d2q(value, maxPrecision);
}

Rational Rational::fromDouble(double value, int maxPrecision)
{
    Rational result(av_d2q(value, maxPrecision));
    return result;
}

int64_t Rational::rescale(int64_t srcValue, const Rational &dstBase) const
{
    return av_rescale_q(srcValue, value, dstBase.getValue());
}

void Rational::dump() const
{
    cout << value.num << "/" << value.den << endl;
}

Rational &Rational::operator =(const AVRational &value)
{
    this->value = value;
    return *this;
}

Rational &Rational::operator =(double value)
{
    this->value = av_d2q(value, RationalMaxPrecision);
    return *this;
}

Rational Rational::operator +(const Rational &value)
{
    Rational result = av_add_q(this->value, value.getValue());
    return result;
}

Rational Rational::operator -(const Rational &value)
{
    Rational result = av_sub_q(this->value, value.getValue());
    return result;
}

Rational Rational::operator *(const Rational &value)
{
    Rational result = av_mul_q(this->value, value.getValue());
    return result;
}

Rational Rational::operator /(const Rational &value)
{
    Rational result = av_div_q(this->value, value.getValue());
    return result;
}

double Rational::operator ()() const
{
    return av_q2d(value);
}

bool Rational::operator ==(const Rational &other) const
{
    // HACK: in this case av_cmp_q() return INT_MIN;
    if (value.den == 0 &&
        value.num == 0 &&
        other.getDenominator() == 0 &&
        other.getNumerator()   == 0)
    {
        return true;
    }

    return (av_cmp_q(value, other.getValue()) == 0 ? true : false);
}

bool Rational::operator <(const Rational &other) const
{
    return (av_cmp_q(value, other.getValue()) == -1 ? true : false);
}


} // ::av
