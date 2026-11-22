#pragma once

/**
 * A single filter instance and its pads.
 *
 * @file
 * @ingroup filter
 */

#include <memory>

#include "avcpp/ffmpeg.h"
#include "filterpad.h"

#if AVCPP_HAS_AVFILTER

extern "C" {
#include <libavfilter/avfilter.h>
}

namespace av {

/**
 * Media type a filter works with.
 *
 * Filters are typed: a video filter rejects audio frames and vice versa, which
 * is what this value records when FFmpeg does not state it explicitly.
 */
enum class FilterMediaType
{
    /// The filter did not declare a type, or it was not inspected yet.
    Unknown,
    /// The filter processes video frames.
    Video,
    /// The filter processes audio samples.
    Audio,
};


/**
 * A filter descriptor: the static description of one filter.
 *
 * A descriptor says what the filter is called, what it does, how many inputs and
 * outputs it has and which flags it sets. It is not a filter instance: to use
 * one, add it to a FilterGraph.
 */
class Filter : public FFWrapperPtr<const AVFilter>
{
public:
    using FFWrapperPtr<const AVFilter>::FFWrapperPtr;

    Filter() = default;
    /**
     * Wrap an FFmpeg filter descriptor.
     *
     * @param ptr the descriptor; the library owns it
     */
    Filter(const AVFilter *ptr);
    /**
     * Look a filter up by name.
     *
     * @param name the filter name, for example "scale"
     */
    explicit Filter(const std::string& name);
    /**
     * Look a filter up by name.
     *
     * @param name the filter name, for example "scale"
     */
    explicit Filter(const char* name);

    /**
     * Point the wrapper at another filter.
     *
     * @param name the filter name
     *
     * @return `true` when the name was found
     */
    bool setFilter(const std::string &name);
    /**
     * Point the wrapper at another filter.
     *
     * @param name the filter name
     *
     * @return `true` when the name was found
     */
    bool setFilter(const char* name);

    /**
     * Name of the filter.
     *
     * @return the name, or an empty string when the wrapper is empty
     */
    std::string name()        const;
    /**
     * Description of what the filter does.
     *
     * @return the description text, or an empty string when the wrapper is empty
     */
    std::string description() const;

    /**
     * Inputs of the filter.
     *
     * @return a non-owning list of pads
     */
    FilterPadList inputs()    const;
    /**
     * Outputs of the filter.
     *
     * @return a non-owning list of pads
     */
    FilterPadList outputs()   const;

    /**
     * `AVFILTER_FLAG_*` flags of the filter.
     *
     * @return the flag mask
     */
    int flags() const;

    /**
     * Whether the wrapper points at a filter.
     *
     * @return `true` when a descriptor is set
     */
    operator bool() const;
};


} // namespace av

#endif // if AVCPP_HAS_AVFILTER
