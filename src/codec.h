#ifndef CODEC_H
#define CODEC_H

#include <list>
#include <deque>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "format.h"

namespace av {

class Codec : public FFWrapperPtr<const AVCodec>
{
public:
    using FFWrapperPtr<const AVCodec>::FFWrapperPtr;

    const char *name() const;
    const char *longName() const;
    bool        canEncode() const;
    bool        canDecode() const;
    AVMediaType type() const;

    std::deque<Rational>       supportedFramerates()    const;
    std::deque<AVPixelFormat>  supportedPixelFormats()  const;
    std::deque<int>            supportedSamplerates()   const;
    std::deque<AVSampleFormat> supportedSampleFormats() const;
    std::deque<uint64_t>       supportedChannelLayouts() const;

    AVCodecID id() const;
};


Codec findEncodingCodec(const OutputFormat &format, bool isVideo = true);
Codec findEncodingCodec(AVCodecID id);
Codec findEncodingCodec(const std::string& name);

Codec findDecodingCodec(AVCodecID id);
Codec findDecodingCodec(const std::string& name);

Codec guessEncodingCodec(OutputFormat format, const char *name, const char *url, const char* mime, AVMediaType mediaType);

} // ::fmpeg

#endif // CODEC_H
