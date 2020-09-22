#ifndef AV_FORMAT_H
#define AV_FORMAT_H

#include <memory>
#include <string>

#include "ffmpeg.h"

namespace av {

using FmtCodec = class Codec;
namespace internal {
bool codec_supported(const AVCodecTag *const *codecTag, const FmtCodec &codec);
} // ::internal

template<typename T>
struct Format : public FFWrapperPtr<T>
{
    using FFWrapperPtr<T>::FFWrapperPtr;

    const char* name() const noexcept {
        return RAW_GET(name, nullptr);
    }

    const char* longName() const noexcept {
        return RAW_GET(long_name, nullptr);
    }

    int32_t flags() const noexcept {
        return RAW_GET(flags, 0);
    }

    bool isFlags(int32_t flags) const noexcept {
        if (m_raw) {
            return (m_raw->flags & flags);
        }
        return false;
    }

    void setFormat(const T* format) noexcept {
        reset(format);
    }

    bool codecSupported(const class Codec &codec) const noexcept
    {
        if (!m_raw)
            return false;
        return internal::codec_supported(m_raw->codec_tag, codec);
    }

protected:
    using FFWrapperPtr<T>::m_raw;
};

class InputFormat : public Format<const AVInputFormat>
{
public:
    using Format<const AVInputFormat>::Format;
    using Format<const AVInputFormat>::setFormat;

    InputFormat() = default;

    InputFormat(const std::string& name) noexcept;

    bool setFormat(const std::string& name) noexcept;

};


class OutputFormat : public Format<const AVOutputFormat>
{
public:
    using Format<const AVOutputFormat>::Format;
    using Format<const AVOutputFormat>::setFormat;

    OutputFormat() = default;

    OutputFormat(const std::string& name,
                 const std::string& url  = std::string(),
                 const std::string& mime = std::string()) noexcept;

    bool        setFormat(const std::string& name,
                          const std::string& url  = std::string(),
                          const std::string& mime = std::string()) noexcept;

    AVCodecID   defaultVideoCodecId() const noexcept;
    AVCodecID   defaultAudioCodecId() const noexcept;
};


OutputFormat guessOutputFormat(const std::string& name,
                               const std::string& url  = std::string(),
                               const std::string& mime = std::string());

} // namespace av

#endif // AV_FORMAT_H
