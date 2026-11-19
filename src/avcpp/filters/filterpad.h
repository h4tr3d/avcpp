#pragma once

/**
 * Filter pads and the links between them.
 *
 * @file
 * @ingroup filter
 */

#include <iterator>
#include <memory>
#include <type_traits>

#include "avcpp/ffmpeg.h"
#include "avcpp/avutils.h"

#if AVCPP_HAS_AVFILTER

namespace av {

/**
 * Pads of a filter: its inputs or its outputs.
 *
 * Pads are the typed endpoints a filter exposes; the list is a non-owning view
 * into the filter's static description, valid while the filter descriptor
 * lives.
 */
class FilterPadList : public FFWrapperPtr<const AVFilterPad>
{
private:
    friend class Filter;
    FilterPadList(const AVFilterPad *begin, size_t count);

public:
    using FFWrapperPtr<const AVFilterPad>::FFWrapperPtr;

    /**
     * Number of pads.
     *
     * @return the pad count
     */
    size_t count() const noexcept;
    /**
     * Name of a pad.
     *
     * @param index zero-based pad index
     *
     * @return the name, or an empty string when the index is out of range
     */
    std::string name(size_t index) const noexcept;
    /**
     * Name of a pad as a C string.
     *
     * @param index zero-based pad index
     *
     * @return a static string owned by FFmpeg, or nullptr when the index is out
     *         of range
     */
    const char* nameCStr(size_t index) const noexcept;
    /**
     * Media type a pad accepts or produces.
     *
     * @param index zero-based pad index
     *
     * @return the `AVMEDIA_TYPE_*` value
     */
    AVMediaType type(size_t index) const noexcept;

private:
    size_t m_count = 0;
};

} // namespace av

#endif // if AVCPP_HAS_AVFILTER
