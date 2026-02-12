#ifdef _WIN32
#  include <windows.h>
#endif

#include <signal.h>

#include "avcompat.h"
#include "av.h"
#include "avutils.h"
#include "packet.h"
#include "frame.h"

extern "C" {
#if AVCPP_HAS_AVDEVICE
#include <libavdevice/avdevice.h>
#endif
}

using namespace std;

//
// Functions
//
namespace av {

void set_logging_level(int32_t level)
{
    if (level < AV_LOG_PANIC)
        av_log_set_level(AV_LOG_QUIET);
    else if (level < AV_LOG_FATAL)
        av_log_set_level(AV_LOG_PANIC);
    else if (level < AV_LOG_ERROR)
        av_log_set_level(AV_LOG_FATAL);
    else if (level < AV_LOG_WARNING)
        av_log_set_level(AV_LOG_ERROR);
    else if (level < AV_LOG_INFO)
        av_log_set_level(AV_LOG_WARNING);
    else if (level < AV_LOG_VERBOSE)
        av_log_set_level(AV_LOG_INFO);
    else if (level < AV_LOG_DEBUG)
        av_log_set_level(AV_LOG_VERBOSE);
    else
        av_log_set_level(AV_LOG_DEBUG);
}


void set_logging_level(const string &level)
{
    if (level == "quiet")
        av_log_set_level(AV_LOG_QUIET);
    else if (level == "panic")
        av_log_set_level(AV_LOG_PANIC);
    else if (level == "fatal")
        av_log_set_level(AV_LOG_FATAL);
    else if (level == "error")
        av_log_set_level(AV_LOG_ERROR);
    else if (level == "warning")
        av_log_set_level(AV_LOG_WARNING);
    else if (level == "info")
        av_log_set_level(AV_LOG_INFO);
    else if (level == "verbose")
        av_log_set_level(AV_LOG_VERBOSE);
    else if (level == "debug")
        av_log_set_level(AV_LOG_DEBUG);
    else
    {
        try
        {
            set_logging_level(lexical_cast<int32_t>(level));
        }
        catch (...)
        {}
    }
}


void dumpBinaryBuffer(uint8_t *buffer, int buffer_size, int width)
{
    bool needNewLine = false;
    for (int i = 0; i < buffer_size; ++i)
    {
        needNewLine = true;
        printf("%.2X", buffer[i]);
        if ((i + 1) % width == 0 && i != 0)
        {
            printf("\n");
            needNewLine = false;
        }
        else
        {
            printf(" ");
        }
    }

    if (needNewLine)
    {
        printf("\n");
    }
}

#if AVCPP_AVCODEC_VERSION_MAJOR < 58 // FFmpeg 4.0
#if !defined(__MINGW32__) || defined(_GLIBCXX_HAS_GTHREADS)
static int avcpp_lockmgr_cb(void **ctx, enum AVLockOp op)
{
    if (!ctx)
        return 1;

    std::mutex *mutex = static_cast<std::mutex*>(*ctx);

    int ret = 0;
    switch (op)
    {
        case AV_LOCK_CREATE:
            mutex = new std::mutex();
            *ctx  = mutex;
            ret   = !mutex;
            break;
        case AV_LOCK_OBTAIN:
            if (mutex)
                mutex->lock();
            break;
        case AV_LOCK_RELEASE:
            if (mutex)
                mutex->unlock();
            break;
        case AV_LOCK_DESTROY:
            delete mutex;
            *ctx = 0;
            break;
    }

    return ret;
}
#elif _WIN32
// MinGW with Win32 thread model
static int avcpp_lockmgr_cb(void **ctx, enum AVLockOp op)
{
    if (!ctx)
        return 1;

    CRITICAL_SECTION *sec = static_cast<CRITICAL_SECTION*>(*ctx);

    int ret = 0;
    switch (op)
    {
        case AV_LOCK_CREATE:
            sec = new CRITICAL_SECTION;
            InitializeCriticalSection(sec);
            *ctx = sec;
            break;
        case AV_LOCK_OBTAIN:
            if (sec)
                EnterCriticalSection(sec);
            break;
        case AV_LOCK_RELEASE:
            if (sec)
                LeaveCriticalSection(sec);
            break;
        case AV_LOCK_DESTROY:
            if (ctx) {
                DeleteCriticalSection(sec);
                delete sec;
            }
            *ctx = nullptr;
            break;
    }

    return ret;
}
#else
#  error "Unknown Threading model"
#endif
#endif

void init()
{
#if AVCPP_HAS_AVFORMAT
#if AVCPP_AVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
    av_register_all();
#endif
    avformat_network_init();
#endif // if AVCPP_HAS_AVFORMAT

#if AVCPP_AVCODEC_VERSION_MAJOR < 58 // FFmpeg 4.0
    avcodec_register_all();
#endif

#if AVCPP_HAS_AVFILTER
#if AVCPP_AVFILTER_VERSION_MAJOR < 7 // FFmpeg 4.0
    avfilter_register_all();
#endif
#endif // if AVCPP_HAS_AVFILTER

#if AVCPP_HAS_AVDEVICE
    avdevice_register_all();
#endif

#if AVCPP_AVCODEC_VERSION_MAJOR < 58 // FFmpeg 4.0
    av_lockmgr_register(avcpp_lockmgr_cb);
#endif

    set_logging_level(AV_LOG_ERROR);

    // Ignore sigpipe by default
#ifdef __unix__
    signal(SIGPIPE, SIG_IGN);
#endif
}


string error2string(int error)
{
    char errorBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(error, errorBuf, AV_ERROR_MAX_STRING_SIZE);
    return string(errorBuf);
}

namespace v1 {
bool AvDeleter::operator()(SwsContext *&swsContext)
{
    sws_freeContext(swsContext);
    swsContext = nullptr;
    return true;
}

bool AvDeleter::operator()(AVCodecContext *&codecContext)
{
#if AVCPP_AVCODEC_VERSION_MAJOR >= 58
    avcodec_free_context(&codecContext);
#else
    avcodec_close(codecContext);
    av_free(codecContext);
    codecContext = nullptr;
#endif
    return true;
}

#if AVCPP_HAS_AVFORMAT
bool AvDeleter::operator()(AVOutputFormat *&format)
{
    // Only set format to zero, it can'be freed by user
    format = 0;
    return true;
}

bool AvDeleter::operator()(AVFormatContext *&formatContext)
{
    avformat_free_context(formatContext);
    formatContext = nullptr;
    return true;
}
#endif // if AVCPP_HAS_AVFORMAT

bool AvDeleter::operator()(AVFrame *&frame)
{
    av_frame_free(&frame);
    return true;
}

bool AvDeleter::operator()(AVPacket *&packet)
{
    av_packet_free(&packet);
    return true;
}

bool AvDeleter::operator()(AVDictionary *&dictionary)
{
    av_dict_free(&dictionary);
    dictionary = nullptr;
    return true;
}

#if AVCPP_HAS_AVFILTER
bool AvDeleter::operator ()(AVFilterInOut *&filterInOut)
{
    avfilter_inout_free(&filterInOut);
    return true;
}
#endif // if AVCPP_HAS_AVFILTER

} // ::v1

#if !AVCPP_HAS_AVFORMAT
// From FFmpeg libavformat/dump.c
#define HEXDUMP_PRINT(...)                                                    \
    do {                                                                      \
        if (!f)                                                               \
            av_log(avcl, level, __VA_ARGS__);                                 \
        else                                                                  \
            fprintf(f, __VA_ARGS__);                                          \
    } while (0)

static void hex_dump_internal(void *avcl, FILE *f, int level, const uint8_t *buf, int size)
{
    int len, i, j, c;

    for (i = 0; i < size; i += 16) {
        len = size - i;
        if (len > 16)
            len = 16;
        HEXDUMP_PRINT("%08x ", i);
        for (j = 0; j < 16; j++) {
            if (j < len)
                HEXDUMP_PRINT(" %02x", buf[i + j]);
            else
                HEXDUMP_PRINT("   ");
        }
        HEXDUMP_PRINT(" ");
        for (j = 0; j < len; j++) {
            c = buf[i + j];
            if (c < ' ' || c > '~')
                c = '.';
            HEXDUMP_PRINT("%c", c);
        }
        HEXDUMP_PRINT("\n");
    }
}

void hex_dump(FILE *f, const uint8_t *buf, std::size_t size)
{
    hex_dump_internal(nullptr, f, 0, buf, size);
}

void hex_dump_log(void *avcl, int level, const uint8_t *buf, std::size_t size)
{
    hex_dump_internal(avcl, nullptr, level, buf, size);
}
#else
void hex_dump(FILE *f, const uint8_t *buf, std::size_t size)
{
    av_hex_dump(f, buf, int(size));
}

void hex_dump_log(void *avcl, int level, const uint8_t *buf, std::size_t size)
{
    av_hex_dump_log(avcl, level, buf, int(size));
}
#endif

#if AVCPP_CXX_STANDARD >= 20
void hex_dump(FILE *f, std::span<const uint8_t> buf)
{
    hex_dump(f, buf.data(), buf.size());
}

void hex_dump_log(void *avcl, int level, std::span<const uint8_t> buf)
{
    hex_dump_log(avcl, level, buf.data(), buf.size());
}
#endif // if AVCPP_CXX_STANDARD >= 20

} // ::av


