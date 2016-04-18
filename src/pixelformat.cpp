#include "pixelformat.h"

namespace av {

PixelFormat::PixelFormat(AVPixelFormat pixfmt) noexcept
    : m_pixfmt(pixfmt)
{
}

PixelFormat::PixelFormat(const char *name) noexcept
    : m_pixfmt(av_get_pix_fmt(name))
{
}

PixelFormat::PixelFormat(const std::string &name) noexcept
    : PixelFormat(name.c_str())
{
}

AVPixelFormat PixelFormat::get() const noexcept
{
    return m_pixfmt;
}

void PixelFormat::set(AVPixelFormat pixfmt) noexcept
{
    m_pixfmt = pixfmt;
}

PixelFormat PixelFormat::fromString(const char *name)
{
    return PixelFormat(name);
}

PixelFormat PixelFormat::fromString(const std::string &name)
{
    return PixelFormat(name);
}

const char *PixelFormat::name(std::error_code &ec) const
{
    if (auto nm = av_get_pix_fmt_name(m_pixfmt))
    {
        clear_if(ec);
        return nm;
    }
    throws_if(ec, std::errc::invalid_argument);
    return nullptr;
}

const AVPixFmtDescriptor *PixelFormat::descriptor(std::error_code &ec) const
{
    if (auto desc = av_pix_fmt_desc_get(m_pixfmt))
    {
        clear_if(ec);
        return desc;
    }
    throws_if(ec, std::errc::invalid_argument);
    return nullptr;
}

int PixelFormat::bitsPerPixel(std::error_code &ec) const
{
    if (auto desc = descriptor(ec))
        return av_get_bits_per_pixel(desc);
    return 0;
}

size_t PixelFormat::planesCount(std::error_code &ec) const
{
    auto count = av_pix_fmt_count_planes(m_pixfmt);
    if (count < 0)
    {
        throws_if(ec, count, ffmpeg_category());
        return 0;
    }
    clear_if(ec);
    return static_cast<size_t>(count);
}

PixelFormat PixelFormat::swapEndianness() const noexcept
{
    return av_pix_fmt_swap_endianness(m_pixfmt);
}

size_t PixelFormat::convertionLoss(PixelFormat dstFmt, bool srcHasAlpha) const noexcept
{
    return static_cast<size_t>(av_get_pix_fmt_loss(dstFmt, m_pixfmt, srcHasAlpha));
}

PixelFormat &PixelFormat::operator=(AVPixelFormat pixfmt) noexcept
{
    m_pixfmt = pixfmt;
    return *this;
}

PixelFormat::operator AVPixelFormat &() noexcept
{
    return m_pixfmt;
}

PixelFormat::operator AVPixelFormat() const noexcept
{
    return m_pixfmt;
}


} // namespace av
