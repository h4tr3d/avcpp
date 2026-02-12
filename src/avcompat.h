#pragma once

#include "avconfig.h"

extern "C" {
#ifdef AVCPP_AVCODEC_VERSION_MAJOR
#  include <libavcodec/avcodec.h>
#endif
#if AVCPP_HAS_AVFILTER
#  include <libavfilter/avfilter.h>
#if AVCPP_AVFILTER_VERSION_INT < AV_VERSION_INT(7,0,0)
#  include <libavfilter/avfiltergraph.h>
#endif
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#if AVCPP_AVFILTER_VERSION_INT <= AV_VERSION_INT(2,77,100) // 0.11.1
#  include <libavfilter/vsrc_buffer.h>
#endif
#if AVCPP_AVFILTER_VERSION_INT < AV_VERSION_INT(6,31,100) // 3.0
#include <libavfilter/avcodec.h>
#endif
#endif // if AVCPP_HAS_AVFILTER
} // extern "C"


// Codec parameters interface
#define AVCPP_USE_CODECPAR ((AVCPP_AVCODEC_VERSION_MAJOR) >= 59) // FFmpeg 5.0

// New Audio Channel Layout API
#define AVCPP_API_NEW_CHANNEL_LAYOUT ((AVCPP_AVUTIL_VERSION_MAJOR > 57) || (AVCPP_AVUTIL_VERSION_MAJOR == 57 && (AVCPP_AVUTIL_VERSION_MINOR >= 24)))
// `int64_t frame_num` has been added in the 60.2, in the 61.0 it should be removed
#define AVCPP_API_FRAME_NUM          ((AVCPP_AVCODEC_VERSION_MAJOR > 60) || (AVCPP_AVCODEC_VERSION_MAJOR == 60 && AVCPP_AVCODEC_VERSION_MINOR >= 2))
// use AVFormatContext::url
#if AVCPP_HAS_AVFORMAT
#  define AVCPP_API_AVFORMAT_URL       ((AVCPP_AVFORMAT_VERSION_MAJOR > 58) || (AVCPP_AVFORMAT_VERSION_MAJOR == 58 && AVCPP_AVFORMAT_VERSION_MINOR >= 7))
#endif // if AVCPP_HAS_AVFORMAT
// net key frame flags: AV_FRAME_FLAG_KEY (FFmpeg 6.1)
#define AVCPP_API_FRAME_KEY          ((AVCPP_AVUTIL_VERSION_MAJOR > 58) || (AVCPP_AVUTIL_VERSION_MAJOR == 58 && AVCPP_AVUTIL_VERSION_MINOR >= 29))
// avcodec_close() support
#define AVCPP_API_AVCODEC_CLOSE      (AVCPP_AVCODEC_VERSION_MAJOR < 61)

// sizeof(AVPacket) is no part of the public ABI, packet must be allocated in heap
#define AVCPP_API_AVCODEC_NEW_INIT_PACKET (AVCPP_AVCODEC_VERSION_MAJOR >= 58)
// some fields in the AVCodec structure deprecard and replaced by the call of avcodec_get_supported_config()
#define AVCPP_API_AVCODEC_GET_SUPPORTED_CONFIG (AVCPP_AVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100))
#if AVCPP_HAS_AVFORMAT
// av_stream_get_codec_timebase() deprecard now without replacement
#  define AVCPP_API_AVFORMAT_AV_STREAM_GET_CODEC_TIMEBASE (AVCPP_AVFORMAT_VERSION_INT < AV_VERSION_INT(61, 5, 101))
#endif // if AVCPP_HAS_AVFORMAT
// AVBuffer API switch to size_t
#define AVCPP_API_AVBUFFER_SIZE_T (AVCPP_AVUTIL_VERSION_MAJOR >= 57)

#if defined(__ICL) || defined (__INTEL_COMPILER)
#    define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:1478))
#    define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#elif defined(_MSC_VER)
#    define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:4996))
#    define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#elif defined(__GNUC__) || defined(__clang__)
#    define FF_DISABLE_DEPRECATION_WARNINGS _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#    define FF_ENABLE_DEPRECATION_WARNINGS  _Pragma("GCC diagnostic warning \"-Wdeprecated-declarations\"")
#else
#    define FF_DISABLE_DEPRECATION_WARNINGS
#    define FF_ENABLE_DEPRECATION_WARNINGS
#endif

// Allow to use spece-ship operator, whem possible
#define AVCPP_USE_SPACESHIP_OPERATOR 0
#if __has_include(<compare>)
#  include <compare>
#  if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907
#    undef  AVCPP_USE_SPACESHIP_OPERATOR
#    define AVCPP_USE_SPACESHIP_OPERATOR 1
#  endif
#endif


// avcodec
#if AVCPP_AVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
inline void avcodec_free_frame(AVFrame **frame)
{
    av_freep(frame);
}
#endif

// avfilter
#if AVCPP_HAS_AVFILTER
#if AVCPP_AVFILTER_VERSION_INT < AV_VERSION_INT(3,17,100) // 1.0
inline const char *avfilter_pad_get_name(AVFilterPad *pads, int pad_idx)
{
    return pads[pad_idx].name;
}

inline AVMediaType avfilter_pad_get_type(AVFilterPad *pads, int pad_idx)
{
    return pads[pad_idx].type;
}
#endif
#endif // if AVCPP_HAS_AVFILTER

// Wrapper around av_free_packet()/av_packet_unref()
#if AVCPP_AVCODEC_VERSION_INT < AV_VERSION_INT(58,8,100) // < 3.0
#define avpacket_unref(p) av_free_packet(p)
#else
#define avpacket_unref(p) av_packet_unref(p)
#endif