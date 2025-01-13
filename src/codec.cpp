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
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,37,100) // 7fc329e
    if (m_raw)
        return m_raw->encode2;
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59,37,100) // <5.1
    if (m_raw)
        return (m_raw->receive_packet || m_raw->encode2);
#else
    return av_codec_is_encoder(m_raw);
#endif
    return false;
}

bool Codec::canDecode() const
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,37,100) // 7fc329e
    if (m_raw)
        return m_raw->decode;
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59,37,100) // <5.1
    if (m_raw)
        return (m_raw->receive_frame || m_raw->decode);
#else
    return av_codec_is_decoder(m_raw);
#endif
    return false;
}

bool Codec::isEncoder() const
{
    return canEncode();
}

bool Codec::isDecoder() const
{
    return canDecode();
}

AVMediaType Codec::type() const
{
    return RAW_GET(type, AVMEDIA_TYPE_UNKNOWN);
}

std::deque<Rational> Codec::supportedFramerates() const
{
    if (!m_raw)
        return {};
    deque<Rational> frameRates;
    const AVRational *frameRatesRaw = nullptr;
#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_FRAME_RATE, 0, reinterpret_cast<const void**>(&frameRatesRaw), nullptr);
#else
    frameRatesRaw = m_raw->supported_framerates;
#endif

    if (!frameRatesRaw)
        return {};

    array_to_container(frameRatesRaw, frameRates, [](const Rational& rate) {
        return rate == Rational();
    });
    return frameRates;
}

std::deque<PixelFormat> Codec::supportedPixelFormats() const
{
    if (!m_raw)
        return {};

    deque<PixelFormat> pixFmts;
    const enum AVPixelFormat *pixFmtsRaw = nullptr;

#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_PIX_FORMAT, 0, reinterpret_cast<const void**>(&pixFmtsRaw), nullptr);
#else
    pixFmtsRaw = m_raw->pix_fmts;
#endif

    if (!pixFmtsRaw)
        return {};

    array_to_container(pixFmtsRaw, pixFmts, [](PixelFormat pixFmt) {
        return pixFmt == AV_PIX_FMT_NONE;
    });

    return pixFmts;
}

std::deque<int> Codec::supportedSamplerates() const
{
    if (!m_raw)
        return {};

    deque<int> sampleRates;
    const int *sampleRatesRaw = nullptr;

#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_SAMPLE_RATE, 0, reinterpret_cast<const void**>(&sampleRatesRaw), nullptr);
#else
    sampleRatesRaw = m_raw->supported_samplerates;
#endif

    if (!sampleRatesRaw)
        return {};

    array_to_container(sampleRatesRaw, sampleRates, [](int sampleRate) {
        return sampleRate == 0;
    });

    return sampleRates;
}

std::deque<SampleFormat> Codec::supportedSampleFormats() const
{
    if (!m_raw)
        return {};

    deque<SampleFormat> sampleFmts;
    const enum AVSampleFormat *sampleFmtsRaw = nullptr;

#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, reinterpret_cast<const void**>(&sampleFmtsRaw), nullptr);
#else
    sampleFmtsRaw = m_raw->sample_fmts;
#endif

    if (!sampleFmtsRaw)
        return {};

    array_to_container(sampleFmtsRaw, sampleFmts, [](SampleFormat sampleFmt) {
        return sampleFmt == AV_SAMPLE_FMT_NONE;
    });

    return sampleFmts;
}

std::deque<uint64_t> Codec::supportedChannelLayouts() const
{
    if (!m_raw)
        return {};

    deque<uint64_t> channelLayouts;

#if API_NEW_CHANNEL_LAYOUT
    const AVChannelLayout *channelLayoutsRaw = nullptr;

#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, reinterpret_cast<const void**>(&channelLayoutsRaw), nullptr);
#else
    channelLayoutsRaw = m_raw->ch_layouts;
#endif

    if (!channelLayoutsRaw)
        return {};

    array_to_container(channelLayoutsRaw, channelLayouts, [](auto& layout) {
            static const AVChannelLayout zeroed{};
            return !memcmp(&layout, &zeroed, sizeof(zeroed));
        }, [](auto &layout) {
            return layout.order == AV_CHANNEL_ORDER_NATIVE ? layout.u.mask : 0;
        });
#else
    array_to_container(m_raw->channel_layouts, channelLayouts, [](uint64_t layout) {
        return layout == 0;
    });
#endif

    return channelLayouts;
}

#if API_NEW_CHANNEL_LAYOUT
std::deque<ChannelLayoutView> Codec::supportedChannelLayouts2() const
{
    if (!m_raw)
        return {};

    deque<ChannelLayoutView> channelLayouts;
    const AVChannelLayout *channelLayoutsRaw = nullptr;

#if API_AVCODEC_GET_SUPPORTED_CONFIG
    avcodec_get_supported_config(nullptr, m_raw, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, reinterpret_cast<const void**>(&channelLayoutsRaw), nullptr);
#else
    channelLayoutsRaw = m_raw->ch_layouts;
#endif

    array_to_container(channelLayoutsRaw, channelLayouts, [](const auto& layout) {
        return layout.nb_channels == 0;
    });

    return channelLayouts;
}
#endif

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
