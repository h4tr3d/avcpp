#ifndef AV_FILTER_H
#define AV_FILTER_H

#include <memory>

#include "ffmpeg.h"
#include "filterpad.h"

extern "C" {
#include <libavfilter/avfilter.h>
}

namespace av {

enum class FilterMediaType
{
    Unknown,
    Video,
    Audio,
};


class Filter : public FFWrapperPtr<const AVFilter>
{
public:
    using FFWrapperPtr<const AVFilter>::FFWrapperPtr;

    Filter() = default;
    Filter(const AVFilter *ptr);
    explicit Filter(const std::string& name);
    explicit Filter(const char* name);

    bool setFilter(const std::string &name);
    bool setFilter(const char* name);

    std::string name()        const;
    std::string description() const;

    FilterPadList inputs()    const;
    FilterPadList outputs()   const;

    int flags() const;

    operator bool() const;
};


} // namespace av

#endif // AV_FILTER_H
