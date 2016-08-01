#ifndef AVLOG_H
#define AVLOG_H

#include <stdarg.h>

#include "ffmpeg.h"

#ifdef __PRETTY_FUNCTION__
#define LOGGED_NAME __PRETTY_FUNCTION__
#else
// should exist on C++11 compiler
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

#endif // LOG_H
