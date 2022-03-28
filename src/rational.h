#ifndef RATIONAL_H
#define RATIONAL_H

#include <utility>
#include <iostream>
#include <memory>

#include "ffmpeg.h"

namespace av
{

#define AV_TIME_BASE_Q_CPP          AVRational{1, AV_TIME_BASE}

enum
{
    RationalMaxPrecision = 5
};

class Rational
{
public:
    Rational() noexcept;
    Rational(int numerator, int denominator = 1) noexcept;
    Rational(const AVRational &value) noexcept;
    Rational(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    AVRational& getValue() noexcept { return m_value; }
    const AVRational& getValue() const noexcept { return m_value; }
    void setValue(const AVRational &newValue) noexcept { m_value = newValue; }

    void setNumerator(int numerator) noexcept { m_value.num = numerator; }
    void setDenominator(int denominator) noexcept { m_value.den = denominator; }

    int    getNumerator() const noexcept{ return m_value.num; }
    int    getDenominator() const noexcept { return m_value.den; }
    double getDouble() const noexcept { return (double)m_value.num / (double)m_value.den; }

    static Rational fromDouble(double value, int maxPrecision = RationalMaxPrecision) noexcept;

    int64_t rescale(int64_t srcValue, const Rational &dstBase) const noexcept;

    void dump() const noexcept;

    Rational& operator=  (const AVRational &value) noexcept;
    Rational& operator=  (double value) noexcept;

    bool      operator== (const Rational   &other) const noexcept;
    bool      operator!= (const Rational   &other) const noexcept;
    bool      operator<  (const Rational   &other) const noexcept;
    Rational  operator+  (const Rational   &value) const noexcept;
    Rational  operator-  (const Rational   &value) const noexcept;
    Rational  operator*  (const Rational   &value) const noexcept;
    Rational  operator/  (const Rational   &value) const noexcept;

    double operator() () const noexcept;

    operator const AVRational&() const noexcept
    {
        return m_value;
    }

private:
    AVRational m_value;
};


inline std::ostream& operator<< (std::ostream &stream, const Rational &value)
{
    stream << value.getNumerator() << '/' << value.getDenominator();
    return stream;

}

inline std::istream& operator>> (std::istream &stream, Rational &value)
{
    char       ch;
    AVRational temp;

    try
    {
        stream >> temp.num >> ch >> temp.den;
        if (ch == '/')
        {
            value.setNumerator(temp.num);
            value.setDenominator(temp.den);
        }
    }
    catch (const std::exception &e)
    {}

    return stream;
}


} // ::av

#endif // RATIONAL_H
