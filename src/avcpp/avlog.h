#pragma once

/**
 * Bridge to the FFmpeg logging system.
 *
 * @file
 * @ingroup core
 */

#include <stdarg.h>

#include "ffmpeg.h"

#ifdef __PRETTY_FUNCTION__
/**
 * Name used to prefix library log messages.
 */
#define LOGGED_NAME __PRETTY_FUNCTION__
#else
// should exist on C++11 compiler
/**
 * Name used to prefix library log messages when the compiler has no
 * __PRETTY_FUNCTION__.
 */
#define LOGGED_NAME __func__
#endif

/// Use null context as logging context
#define null_log(level, format, ...)     av_log(nullptr, level, "%s: " format,  LOGGED_NAME, ##__VA_ARGS__)

/// Use context ptr as logging context
#define ptr_log(level, format, ...)      av_log(m_raw, level, "%s: " format,  LOGGED_NAME, ##__VA_ARGS__)

/// Use context referenct as logging context
#define ref_log(level, format, ...)      av_log(&m_raw, level, "%s: " format,  LOGGED_NAME, ##__VA_ARGS__)

/// Use specific context as logging context for pretty logging
#define ctx_log(ctx, level, format, ...) av_log(ctx, level, "%s: " format,  LOGGED_NAME, ##__VA_ARGS__)

/// Default in-class logger
#define fflog(level, format, ...) _log(level, "%s: " format, LOGGED_NAME, ##__VA_ARGS__)

