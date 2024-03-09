#pragma once

#include "averror.h"
#include "ffmpeg.h"
#include "avutils.h"
#include "codec.h"

extern "C" {
// codec_par.h included if needed
#include <libavcodec/avcodec.h>
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

    // Getters & Setters

    AVMediaType  codecType() const;
    void         codecType(AVMediaType codec_type);

    AVMediaType  mediaType() const;
    void         mediaType(AVMediaType media_type);

    AVCodecID    codecId() const;
    void         codecId(AVCodecID codec_id);

    Codec        encodingCodec() const;
    Codec        decodingCodec() const;

    uint32_t     codecTag() const;
    void         codecTag(uint32_t codec_tag);

    // TBD
};

class CodecParameters : public CodecParametersView, public noncopyable
{
public:
    CodecParameters();
    ~CodecParameters();
};

} // namespace av
