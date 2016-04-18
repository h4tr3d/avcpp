#include "pixelformat.h"

namespace av {

PixelFormat::PixelFormat(const char *name) noexcept
    : Parent(av_get_pix_fmt(name))
{
}

PixelFormat::PixelFormat(const std::string &name) noexcept
    : PixelFormat(name.c_str())
{
}

const char *PixelFormat::name(std::error_code &ec) const
{
    if (auto nm = av_get_pix_fmt_name(m_fmt))
    {
        clear_if(ec);
        return nm;
    }
    throws_if(ec, std::errc::invalid_argument);
    return nullptr;
}

const AVPixFmtDescriptor *PixelFormat::descriptor(std::error_code &ec) const
{
    if (auto desc = av_pix_fmt_desc_get(m_fmt))
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
    auto count = av_pix_fmt_count_planes(m_fmt);
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
    return av_pix_fmt_swap_endianness(m_fmt);
}

size_t PixelFormat::convertionLoss(PixelFormat dstFmt, bool srcHasAlpha) const noexcept
{
    return static_cast<size_t>(av_get_pix_fmt_loss(dstFmt, m_fmt, srcHasAlpha));
}

} // namespace av
