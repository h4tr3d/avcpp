#ifndef AV_FILTER_H
#define AV_FILTER_H

#include <memory>

#include "ffmpeg.h"
#include "filterpad.h"

namespace av {

class Filter
{
public:
    Filter();
    Filter(const std::string& name);
    Filter(const AVFilter *filter);
    Filter(const Filter& other);
    ~Filter();

    const AVFilter* getAVFilter() const;

    bool isValid() const;
    bool setFilter(const std::string &name);
    void setFilter(const AVFilter *filter);

    std::string getName()        const;
    std::string getDescription() const;

    FilterPadList getInputs()    const;
    FilterPadList getOutputs()   const;

private:
    void init();

private:
    const AVFilter *filter;
};


} // namespace av

#endif // AV_FILTER_H
