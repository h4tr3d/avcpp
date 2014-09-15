#include <signal.h>

#include "av.h"
#include "avutils.h"

using namespace std;

//
// Functions
//
namespace av {

void setFFmpegLoggingLevel(int32_t level)
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


void setFFmpegLoggingLevel(const string &level)
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
            setFFmpegLoggingLevel(lexical_cast<int32_t>(level));
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
            *ctx = mutex;
            ret = !!mutex;
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


void init()
{
    av_register_all();
    avformat_network_init();
    avcodec_register_all();
    avfilter_register_all();

    av_lockmgr_register(avcpp_lockmgr_cb);
    setFFmpegLoggingLevel(AV_LOG_ERROR);

    // Ignore sigpipe by default
    signal(SIGPIPE, SIG_IGN);
}

string error2string(int error)
{
    char errorBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(error, errorBuf, AV_ERROR_MAX_STRING_SIZE);
    return string(errorBuf);
}


} // ::av


