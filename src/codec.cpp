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
    deque<Rational> frameRates;
    if (!m_raw)
        return frameRates;
    array_to_container(m_raw->supported_framerates, frameRates, [](const Rational& rate) {
        return rate == Rational();
    });
    return frameRates;
}

std::deque<PixelFormat> Codec::supportedPixelFormats() const
{
    deque<PixelFormat> pixFmts;
    if (!m_raw)
        return pixFmts;

    array_to_container(m_raw->pix_fmts, pixFmts, [](PixelFormat pixFmt) {
        return pixFmt == AV_PIX_FMT_NONE;
    });

    return pixFmts;
}

std::deque<int> Codec::supportedSamplerates() const
{
    deque<int> sampleRates;
    if (!m_raw)
        return sampleRates;

    array_to_container(m_raw->supported_samplerates, sampleRates, [](int sampleRate) {
        return sampleRate == 0;
    });

    return sampleRates;
}

std::deque<SampleFormat> Codec::supportedSampleFormats() const
{
    deque<SampleFormat> sampleFmts;
    if (!m_raw)
        return sampleFmts;

    array_to_container(m_raw->sample_fmts, sampleFmts, [](SampleFormat sampleFmt) {
        return sampleFmt == AV_SAMPLE_FMT_NONE;
    });

    return sampleFmts;
}

std::deque<uint64_t> Codec::supportedChannelLayouts() const
{
    deque<uint64_t> channelLayouts;
    if (!m_raw)
        return channelLayouts;

    array_to_container(m_raw->channel_layouts, channelLayouts, [](uint64_t layout) {
        return layout == 0;
    });

    return channelLayouts;
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
