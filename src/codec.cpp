#include "codec.h"
#include "avutils.h"

namespace av {

Codec::Codec()
{
    codec = 0;
}


Codec::~Codec()
{
    codec = 0;
}


Codec::Codec(AVCodec *codec)
    : codec(codec)
{
}

CodecPtr Codec::findEncodingCodec(AVCodecID id)
{
    CodecPtr result;
    AVCodec *codec = avcodec_find_encoder(id);
    if (codec)
        result = CodecPtr(new Codec(codec));

    return result;
}

CodecPtr av::Codec::findEncodingCodec(const char *name)
{
    CodecPtr result;
    AVCodec *codec = avcodec_find_encoder_by_name(name);
    if (codec)
        result = CodecPtr(new Codec(codec));

    return result;
}

CodecPtr av::Codec::findDecodingCodec(AVCodecID id)
{
    CodecPtr result;
    AVCodec *codec = avcodec_find_decoder(id);
    if (codec)
        result = CodecPtr(new Codec(codec));

    return result;
}

CodecPtr av::Codec::findDecodingCodec(const char *name)
{
    CodecPtr result;
    AVCodec *codec = avcodec_find_decoder_by_name(name);
    if (codec)
        result = CodecPtr(new Codec(codec));

    return result;
}

CodecPtr av::Codec::guessEncodingCodec(const char *name, const char *url, const char *mime)
{
    // TODO: need to complete
    boost::shared_ptr<Codec> result;
    //CodecID codecId = av_guess_codec();
    return result;
}

const char *Codec::getName() const
{
    if (codec)
        return codec->name;
    return 0;
}

const char *Codec::getLongName() const
{
    if (codec)
        return codec->long_name;
    return 0;
}

bool Codec::canEncode() const
{
#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(54,23,100) // 0.11.1
    if (codec)
        return (codec->encode || codec->encode2);
#else
    if (codec)
        return codec->encode2;
#endif
    return false;
}

bool Codec::canDecode() const
{
    if (codec)
        return codec->decode;
    return false;
}

list<Rational> Codec::getSupportedFramerates() const
{
    list<Rational> result;
    if (!codec || !codec->supported_framerates)
    {
        std::cout << "No supported frame rates" << std::endl;
        return result;
    }

    int idx = 0;
    while (Rational(codec->supported_framerates[idx]) != Rational())
    {
        Rational value = codec->supported_framerates[idx];
        std::cout << "Supported frame rate: " << value << std::endl;
        result.push_back(value);
        ++idx;
    }

    return result;
}

list<PixelFormat> Codec::getSupportedPixelFormats() const
{
    list<PixelFormat> result;
    if (!codec || !codec->pix_fmts)
    {
        return result;
    }

    PixelFormat format;
    while ((format = *(codec->pix_fmts++)) != -1)
    {
        result.push_back(format);
    }

    return result;
}


} // ::av
