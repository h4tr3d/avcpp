#ifndef CODEC_H
#define CODEC_H

#include <list>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "format.h"

namespace av {

typedef std::shared_ptr<class Codec> CodecPtr;
typedef std::weak_ptr<class Codec>   CodecWPtr;

class Codec : public FFWrapperPtr<const AVCodec>
{
public:
    using FFWrapperPtr<const AVCodec>::FFWrapperPtr;

    // Compat
    const AVCodec *getAVCodec() const {return m_raw;}

    static CodecPtr findEncodingCodec(AVCodecID id);
    static CodecPtr findEncodingCodec(const char *name);

    static CodecPtr findDecodingCodec(AVCodecID id);
    static CodecPtr findDecodingCodec(const char *name);

    static CodecPtr guessEncodingCodec(const char *name, const char *url, const char* mime);

    const char *getName() const;
    const char *getLongName() const;
    bool        canEncode() const;
    bool        canDecode() const;
    AVMediaType type() const;

    std::list<Rational>    getSupportedFramerates() const;
    std::list<PixelFormat> getSupportedPixelFormats() const;
    
    AVCodecID getId() const;
};


Codec findEncodingCodec(AVCodecID id);
Codec findEncodingCodec(const std::string& name);

Codec findDecodingCodec(AVCodecID id);
Codec findDecodingCodec(const std::string& name);

Codec guessEncodingCodec(OutputFormat format, const char *name, const char *url, const char* mime, AVMediaType mediaType);

} // ::fmpeg

#endif // CODEC_H
