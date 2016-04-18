#ifndef AVLOG_H
#define AVLOG_H

#include <stdarg.h>

#include "ffmpeg.h"

/// Use null context as logging context
#define null_log(level, format, ...)     av_log(nullptr, level, "%s: " format,  __PRETTY_FUNCTION__, ##__VA_ARGS__)

/// Use context ptr as logging context
#define ptr_log(level, format, ...)      av_log(m_raw, level, "%s: " format,  __PRETTY_FUNCTION__, ##__VA_ARGS__)

/// Use context referenct as logging context
#define ref_log(level, format, ...)      av_log(&m_raw, level, "%s: " format,  __PRETTY_FUNCTION__, ##__VA_ARGS__)

/// Use specific context as logging context for pretty logging
#define ctx_log(ctx, level, format, ...) av_log(ctx, level, "%s: " format,  __PRETTY_FUNCTION__, ##__VA_ARGS__)

/// Default in-class logger
#define fflog(level, format, ...) _log(level, "%s: " format, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#endif // LOG_H
