#include "codec.h"
#include "avutils.h"

namespace av {

using namespace std;

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
    std::shared_ptr<Codec> result;
    //CodecID codecId = av_guess_codec();
    return result;
}

const char *Codec::getName() const
{
    return RAW_GET(name, nullptr);
}

const char *Codec::getLongName() const
{
    return RAW_GET(long_name, nullptr);
}

bool Codec::canEncode() const
{
#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(54,23,100) // 0.11.1
    if (m_raw)
        return (m_raw->encode || m_raw->encode2);
#else
    if (m_raw)
        return m_raw->encode2;
#endif
    return false;
}

bool Codec::canDecode() const
{
    return RAW_GET(decode, nullptr);
}

AVMediaType Codec::type() const
{
    return RAW_GET(type, AVMEDIA_TYPE_UNKNOWN);
}

list<Rational> Codec::getSupportedFramerates() const
{
    list<Rational> result;
    if (!m_raw || !m_raw->supported_framerates)
    {
        std::cout << "No supported frame rates" << std::endl;
        return result;
    }

    int idx = 0;
    while (Rational(m_raw->supported_framerates[idx]) != Rational())
    {
        Rational value = m_raw->supported_framerates[idx];
        std::cout << "Supported frame rate: " << value << std::endl;
        result.push_back(value);
        ++idx;
    }

    return result;
}

list<PixelFormat> Codec::getSupportedPixelFormats() const
{
    list<PixelFormat> result;
    if (!m_raw || !m_raw->pix_fmts)
    {
        return result;
    }

    PixelFormat format;
    const AVPixelFormat *pixFmts = m_raw->pix_fmts;
    while ((format = *(pixFmts++)) != -1)
    {
        result.push_back(format);
    }

    return result;
}

AVCodecID Codec::getId() const
{
    return RAW_GET(id, AV_CODEC_ID_NONE);
}

Codec findEncodingCodec(AVCodecID id)
{
    return Codec { avcodec_find_encoder(id) };
}

Codec findEncodingCodec(const string &name)
{
    return Codec { avcodec_find_encoder_by_name(name.c_str()) };
}

Codec findDecodingCodec(AVCodecID id)
{
    return Codec {avcodec_find_decoder(id)};
}

Codec findDecodingCodec(const string &name)
{
    return Codec { avcodec_find_decoder_by_name(name.c_str()) };
}

Codec guessEncodingCodec(OutputFormat format, const char *name, const char *url, const char *mime, AVMediaType mediaType)
{
    auto id = av_guess_codec(format.raw(), name, url, mime, mediaType);
    return findEncodingCodec(id);
}

Codec findEncodingCodec(const OutputFormat &format, bool isVideo)
{
    if (isVideo)
        return Codec { avcodec_find_encoder(format.defaultVideoCodecId()) };
    else
        return Codec { avcodec_find_encoder(format.defaultAudioCodecId()) };
}

} // ::av
