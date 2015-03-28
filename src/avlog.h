#ifndef AVLOG_H
#define AVLOG_H

#include <stdarg.h>

#include "ffmpeg.h"

/// Use null context as logging context
#define null_log(level, format, args...)     av_log(nullptr, level, "%s: " format,  __PRETTY_FUNCTION__, ##args)

/// Use context ptr as logging context
#define ptr_log(level, format, args...)      av_log(m_raw, level, "%s: " format,  __PRETTY_FUNCTION__, ##args)

/// Use context referenct as logging context
#define ref_log(level, format, args...)      av_log(&m_raw, level, "%s: " format,  __PRETTY_FUNCTION__, ##args)

/// Use specific context as logging context for pretty logging
#define ctx_log(ctx, level, format, args...) av_log(ctx, level, "%s: " format,  __PRETTY_FUNCTION__, ##args)

/// Default in-class logger
#define fflog(level, format, args...) _log(level, "%s: " format, __PRETTY_FUNCTION__, ##args)

#endif // LOG_H
