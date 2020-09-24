#pragma once

#include <memory>
#include <string>

#include "ffmpeg.h"

namespace av {

// AVFormat 59 will introduce `const` for all muxer/demuxer description operations
template<typename T>
using avcpp_format_const = typename std::conditional<
    std::is_const<
        typename std::remove_pointer<
            decltype(AVFormatContext::iformat)
        >::type
    >::value, const T, T>::type;

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

    void setFormat(T* format) noexcept {
        FFWrapperPtr<T>::reset(format);
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

class InputFormat : public Format<avcpp_format_const<AVInputFormat>>
{
public:
    using Format<avcpp_format_const<AVInputFormat>>::Format;
    using Format<avcpp_format_const<AVInputFormat>>::setFormat;

    InputFormat() = default;

    InputFormat(const std::string& name) noexcept;

    bool setFormat(const std::string& name) noexcept;

};


class OutputFormat : public Format<avcpp_format_const<AVOutputFormat>>
{
public:
    using Format<avcpp_format_const<AVOutputFormat>>::Format;
    using Format<avcpp_format_const<AVOutputFormat>>::setFormat;

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

