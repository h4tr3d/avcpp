#include "codec.h"
#include "format.h"

namespace av {

namespace internal {

bool codec_supported(const AVCodecTag * const *codecTag, const FmtCodec &codec)
{
    if (!codecTag) // any codec supported
        return true;

    unsigned int tag;
    return !!av_codec_get_tag2(codecTag, codec.id(), &tag);
}

} // ::internal

////////////////////////////////////////////////////////////////////////////////////////////////////
InputFormat::InputFormat(const std::string &name) noexcept
{
    setFormat(name);
}

bool InputFormat::setFormat(const std::string &name) noexcept
{
    if (!name.empty())
        m_raw = av_find_input_format(name.c_str());
    else
        m_raw = nullptr;
    return m_raw;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
OutputFormat::OutputFormat(const std::string &name, const std::string &url, const std::string &mime) noexcept
{
    setFormat(name, url, mime);
}

bool OutputFormat::setFormat(const std::string &name, const std::string &url, const std::string &mime) noexcept
{
    if (name.empty() && url.empty() && mime.empty())
        m_raw = nullptr;
    else
        m_raw = av_guess_format(name.empty() ? nullptr : name.c_str(),
                                url.empty()  ? nullptr : url.c_str(),
                                mime.empty() ? nullptr : mime.c_str());
    return m_raw;
}


AVCodecID OutputFormat::defaultVideoCodecId() const noexcept
{
    return RAW_GET(video_codec, AV_CODEC_ID_NONE);
}


AVCodecID OutputFormat::defaultAudioCodecId() const noexcept
{
    return RAW_GET(audio_codec, AV_CODEC_ID_NONE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
OutputFormat guessOutputFormat(const std::string &name, const std::string &url, const std::string &mime)
{
    return {name, url, mime};
}

} // namespace av
