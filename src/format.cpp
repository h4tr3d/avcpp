#include "format.h"

namespace av {

////////////////////////////////////////////////////////////////////////////////////////////////////
bool InputFormat::setFormat(const std::string &name) {
    if (!name.empty())
        m_raw = av_find_input_format(name.c_str());
    else
        m_raw = nullptr;
    return m_raw;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool OutputFormat::setFormat(const std::string &name, const std::string &url, const std::string &mime)
{
    if (name.empty() && url.empty() && mime.empty())
        m_raw = nullptr;
    else
        m_raw = av_guess_format(name.empty() ? nullptr : name.c_str(),
                                url.empty()  ? nullptr : url.c_str(),
                                mime.empty() ? nullptr : mime.c_str());
    return m_raw;
}

AVCodecID OutputFormat::defaultVideoCodecId() const
{
    return RAW_GET(video_codec, AV_CODEC_ID_NONE);
}

AVCodecID OutputFormat::defaultAudioCodecId() const
{
    return RAW_GET(audio_codec, AV_CODEC_ID_NONE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
OutputFormat guessOutputFormat(const std::string &name, const std::string &url, const std::string &mime)
{
    OutputFormat result;
    result.setFormat(name, url, mime);
    return result;
}


} // namespace av
