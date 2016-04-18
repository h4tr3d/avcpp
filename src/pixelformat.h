#ifndef AV_PIXELFORMAT_H
#define AV_PIXELFORMAT_H

#include <string>
#include <iostream>

#include "averror.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}

// We need not to be back compatible:
#ifdef PixelFormat
#undef PixelFormat
#endif

namespace av {

/**
 * @brief The PixelFormat class is a simple wrapper for AVPixelFormat that allow to acces it it properties
 *
 * It has size same to the AVPixelFormat, but wrap pixel description function too.
 */
class PixelFormat
{
public:
    PixelFormat() = default;
    PixelFormat(AVPixelFormat pixfmt) noexcept;
    explicit PixelFormat(const char* name) noexcept;
    explicit PixelFormat(const std::string& name) noexcept;

    // Access to  the stored value
    operator AVPixelFormat() const noexcept;
    AVPixelFormat get() const noexcept;

    operator AVPixelFormat&() noexcept;
    PixelFormat& operator=(AVPixelFormat pixfmt) noexcept;
    void set(AVPixelFormat pixfmt) noexcept;

    // Additional construction helper
    static PixelFormat fromString(const char* name);
    static PixelFormat fromString(const std::string &name);

    // Info functions
    const char* name(std::error_code &ec = throws()) const;

    const AVPixFmtDescriptor* descriptor(std::error_code &ec = throws()) const;

    int bitsPerPixel(std::error_code &ec = throws()) const;

    size_t planesCount(std::error_code &ec = throws()) const;

    PixelFormat swapEndianness() const noexcept;

    // See FF_LOSS_xxx flags
    size_t convertionLoss(PixelFormat dstFmt, bool srcHasAlpha = false) const noexcept;

private:
    AVPixelFormat m_pixfmt = AV_PIX_FMT_NONE;
};


// IO Stream interface
inline std::ostream& operator<<(std::ostream& ost, PixelFormat fmt)
{
    ost << fmt.name();
    return ost;
}

} // namespace av

#endif // AV_PIXELFORMAT_H
