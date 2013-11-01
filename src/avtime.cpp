#include "avtime.h"

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,73,101) // FFMPEG 1.0
extern "C"
{
  #include <libavutil/time.h>
}
#else
#  include <time.h>
#  if _POSIX_VERSION < 200112L
#    include <unistd.h>
#  elif defined(WIN32)
#    include <windows.h>
#  endif
#endif

namespace av {

// Get current time in us
int64_t gettime()
{
    return av_gettime();
}

// Sleep given amount of us
int usleep(unsigned usec)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,73,101) // FFMPEG 1.0
    return av_usleep(usec);
#else
    // Code taken from FFMPEG 1.0
#if _POSIX_VERSION >= 200112L
    struct timespec ts;
    ts.tv_sec  = usec / 1000000;
    ts.tv_nsec = usec % 1000000 * 1000;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR);
    return 0;
#elif _POSIX_VERSION < 200112L
    return usleep(usec)
#elif defined(WIN32)
    Sleep(usec / 1000);
    return 0;
#else
    return AVERROR(ENOSYS);
#endif
#endif
}




} // ::av
