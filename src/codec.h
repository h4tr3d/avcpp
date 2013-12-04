#ifndef CODEC_H
#define CODEC_H

#include <list>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"

namespace av {

class Codec;

typedef std::shared_ptr<Codec> CodecPtr;
typedef std::weak_ptr<Codec>   CodecWPtr;

class Codec
{
public:
    Codec(AVCodec *codec);
    ~Codec();

    AVCodec *getAVCodec() const {return codec;}

    static CodecPtr findEncodingCodec(AVCodecID id);
    static CodecPtr findEncodingCodec(const char *name);

    static CodecPtr findDecodingCodec(AVCodecID id);
    static CodecPtr findDecodingCodec(const char *name);

    static CodecPtr guessEncodingCodec(const char *name, const char *url, const char* mime);

    const char *getName() const;
    const char *getLongName() const;
    bool        canEncode() const;
    bool        canDecode() const;

    std::list<Rational>    getSupportedFramerates() const;
    std::list<PixelFormat> getSupportedPixelFormats() const;

private:
    Codec();

private:
    AVCodec *codec;
};

} // ::fmpeg

#endif // CODEC_H
