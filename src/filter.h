#ifndef AV_FILTER_H
#define AV_FILTER_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

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

//typedef boost::shared_ptr<Filter> FilterPtr;
//typedef boost::weak_ptr<Filter>   FilterWPtr;

} // namespace av

#endif // AV_FILTER_H
