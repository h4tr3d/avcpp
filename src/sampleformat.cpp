#include "sampleformat.h"

namespace av {

SampleFormat::SampleFormat(const char *name) noexcept
    : Parent(av_get_sample_fmt(name))
{
}

SampleFormat::SampleFormat(const std::string &name) noexcept
    : SampleFormat(name.c_str())
{
}

const char *SampleFormat::name(std::error_code &ec) const
{
    if (auto nm = av_get_sample_fmt_name(m_fmt))
    {
        clear_if(ec);
        return nm;
    }
    throws_if(ec, std::errc::invalid_argument);
    return nullptr;
}

SampleFormat SampleFormat::alternativeSampleFormat(bool isPlanar) const noexcept
{
    return av_get_alt_sample_fmt(m_fmt, isPlanar);
}

SampleFormat SampleFormat::packedSampleFormat() const noexcept
{
    return av_get_packed_sample_fmt(m_fmt);
}

SampleFormat SampleFormat::planarSampleFormat() const noexcept
{
    return av_get_planar_sample_fmt(m_fmt);
}

bool SampleFormat::isPlanar() const noexcept
{
    return av_sample_fmt_is_planar(m_fmt);
}

size_t SampleFormat::bytesPerSample(std::error_code &ec) const
{
    if (auto size = av_get_bytes_per_sample(m_fmt) > 0)
    {
        clear_if(ec);
        return static_cast<size_t>(size);
    }
    throws_if(ec, std::errc::invalid_argument);
    return 0;
}

size_t SampleFormat::requiredBufferSize(int nbChannels, int nbSamples, int align, std::error_code &ec) const
{
    int lineSizeDummy;
    return requiredBufferSize(nbChannels, nbSamples, align, lineSizeDummy, ec);
}

size_t SampleFormat::requiredBufferSize(int nbChannels, int nbSamples, int align, int &lineSize, std::error_code &ec) const
{
    auto sts = av_samples_get_buffer_size(&lineSize, nbChannels, nbSamples, m_fmt, align);
    if (sts < 0)
    {
        throws_if(ec, std::errc::invalid_argument);
        return 0;
    }
    clear_if(ec);
    return static_cast<size_t>(sts);
}


} // namespace av
