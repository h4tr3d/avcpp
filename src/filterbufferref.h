#ifndef AV_FILTERBUFFERREF_H
#define AV_FILTERBUFFERREF_H

#include "ffmpeg.h"
#include "frame.h"
#include "audiosamples.h"
#include "videoframe.h"

namespace av {

// TODO make via shared_ptr and external counter
class FilterBufferRef
{
public:
    FilterBufferRef();
    FilterBufferRef(AVFilterBufferRef *ref, int mask = 0);
    FilterBufferRef(const FilterBufferRef& ref, int mask = 0);
    ~FilterBufferRef();

    FilterBufferRef& operator=(const FilterBufferRef& rhs);
    FilterBufferRef& operator=(AVFilterBufferRef* rhs);

    bool isValid() const;

    AVFilterBufferRef* getAVFilterBufferRef();
    void reset(AVFilterBufferRef *other);

    void copyProps(const FilterBufferRef& other);

    int copyToFrame(FramePtr &dstFrame) const;

    int makeFromFrame(const AudioSamplesPtr& samples, int perms = 0);
    int makeFromFrame(const VideoFramePtr&   frame,   int perms = 0);

    // TODO make RW access to AVFilterBufferRef fields
    AVMediaType getMediaType() const;

private:
    AVFilterBufferRef *ref;
};

} // namespace av

#endif // AV_FILTERBUFFERREF_H
