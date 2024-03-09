#pragma once

#include "averror.h"
#include "ffmpeg.h"
#include "avutils.h"

extern "C" {
#include <libavcodec/codec_par.h>
}


namespace av {

/**
 * @brief The CodecParametersView class
 *
 * Not-owned wrapper for AVCodecParameters. Do not keep for a long time.
 */
class CodecParametersView : public FFWrapperPtr<AVCodecParameters>
{
public:
    CodecParametersView(AVCodecParameters *codecpar = nullptr);
    bool isValid() const;

    void copyFrom(CodecParametersView src, OptionalErrorCode ec = throws());
    void copyTo(CodecParametersView dst, OptionalErrorCode ec = throws()) const;

    void copyFrom(const class CodecContext2& src, OptionalErrorCode ec = throws());
    void copyTo(class CodecContext2& dst, OptionalErrorCode ec = throws()) const;

    int getAudioFrameDuration(int frame_bytes) const;
};

class CodecParameters : CodecParametersView, public noncopyable
{
public:
    CodecParameters();
    ~CodecParameters();
};

} // namespace av
