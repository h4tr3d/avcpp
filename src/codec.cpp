#include "codec.h"
#include "avutils.h"

namespace av {

using namespace std;


const char *Codec::name() const
{
    return RAW_GET(name, nullptr);
}

const char *Codec::longName() const
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

std::deque<Rational> Codec::supportedFramerates() const
{
    if (!m_raw)
        return deque<Rational>();
    return conditional_ends_array_to_deque<AVRational, Rational>(m_raw->supported_framerates, Rational());
}

std::deque<AVPixelFormat> Codec::supportedPixelFormats() const
{
    if (!m_raw)
        return deque<AVPixelFormat>();
    else
        return conditional_ends_array_to_deque(m_raw->pix_fmts, AV_PIX_FMT_NONE);
}

std::deque<int> Codec::supportedSamplerates() const
{
    if (!m_raw)
        return deque<int>();
    else
        return conditional_ends_array_to_deque(m_raw->supported_samplerates, 0);
}

std::deque<AVSampleFormat> Codec::supportedSampleFormats() const
{
    if (!m_raw)
        return deque<AVSampleFormat>();
    else
        return conditional_ends_array_to_deque(m_raw->sample_fmts, AV_SAMPLE_FMT_NONE);
}

std::deque<uint64_t> Codec::supportedChannelLayouts() const
{
    if (!m_raw)
        return deque<uint64_t>();
    else
        return conditional_ends_array_to_deque(m_raw->channel_layouts, uint64_t(0));
}

AVCodecID Codec::id() const
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
