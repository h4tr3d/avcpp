#pragma once

/**
 * Conversions between time units and FFmpeg time bases.
 *
 * @file
 * @ingroup core
 */

#include "ffmpeg.h"

namespace av {

/**
 * Get the current time in microseconds.
 */
/**
 * Monotonic time source used by the library.
 *
 * @return the current time in milliseconds since an unspecified origin
 */
int64_t gettime();


/**
 * Sleep for a period of time.  Although the duration is expressed in
 * microseconds, the actual delay may be rounded to the precision of the
 * system timer.
 *
 * @param  usec Number of microseconds to sleep.
 * @return zero on success or (negative) error code.
 */
int usleep(unsigned usec);


} // ::av

