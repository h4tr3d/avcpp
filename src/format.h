#ifndef AV_FORMAT_H
#define AV_FORMAT_H

#include <memory>

#include "ffmpeg.h"

namespace av {

template<typename T>
struct Format : public FFWrapperPtr<T>
{
    using FFWrapperPtr<T>::FFWrapperPtr;

    const char* formatName() const {
        return RAW_GET(name, nullptr);
    }

    const char* longFormatName() const {
        return RAW_GET(long_name, nullptr);
    }

    int32_t     flags() const {
        return RAW_GET(flags, 0);
    }

    void setFormat(const T* format) {
        reset(format);
    }

protected:
    using FFWrapperPtr<T>::m_raw;
};


class InputFormat : public Format<AVInputFormat>
{
public:
    using Format<AVInputFormat>::Format;
    using Format<AVInputFormat>::setFormat;

    bool setFormat(const std::string& name);

};


class OutputFormat : public Format<AVOutputFormat>
{
public:
    using Format<AVOutputFormat>::Format;
    using Format<AVOutputFormat>::setFormat;

    bool        setFormat(const std::string& name,
                          const std::string& url  = std::string(),
                          const std::string& mime = std::string());

    AVCodecID   defaultVideoCodecId() const;
    AVCodecID   defaultAudioCodecId() const;
};


OutputFormat guessOutputFormat(const std::string& name,
                               const std::string& url  = std::string(),
                               const std::string& mime = std::string());

} // namespace av

#endif // AV_FORMAT_H
