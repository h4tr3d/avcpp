#ifndef CONTAINERFORMAT_H
#define CONTAINERFORMAT_H

#include <memory>

#include "ffmpeg.h"

namespace av
{

class ContainerFormat;

typedef std::shared_ptr<ContainerFormat> ContainerFormatPtr;
typedef std::weak_ptr<ContainerFormat> ContainerFormatWPtr;


class ContainerFormat
{
public:
    ContainerFormat();
    ContainerFormat(AVInputFormat *inputFormat, AVOutputFormat *outputFormat);
    ContainerFormat(const ContainerFormat &format);
    ~ContainerFormat();

    ContainerFormat& operator= (const ContainerFormat &format);


    AVInputFormat*  getInputFormat() const;
    AVOutputFormat* getOutputFormat() const;
    void            setInputFormat(AVInputFormat *format);
    void            setOutputFormat(AVOutputFormat *format);

    bool            setInputFormat(const char *name);
    bool            setOutputFormat(const char *name,
                                    const char *url,
                                    const char *mime);


    const char*     getInputFormatName();
    const char*     getInputFormatLongName();

    const char*     getOutputFormatName();
    const char*     getOutputFormatLongName();
    const char*     getOutputFormatMimeType();

    bool            isInput();
    bool            isOutput();

    int32_t         getInputFlags();
    void            setInputFlags(int32_t flags);
    void            addInputFlags(int32_t flags);
    void            clearInputFlags(int32_t flags);

    int32_t         getOutputFlags();
    void            setOutputFlags(int32_t flags);
    void            addOutputFlags(int32_t flags);
    void            clearOutputFlags(int32_t flags);

    AVCodecID       getOutputDefaultAudioCodec();
    AVCodecID       getOutputDefaultVideoCodec();
    bool            isCodecSupportedForOutput(AVCodecID codecId);

    static std::shared_ptr<ContainerFormat> guessOutputFormat(const char *name,
                                                              const char *url,
                                                              const char *mime);

protected:


private:
    AVInputFormat  *inputFormat;
    AVOutputFormat *outputFormat;
};

} // ::av

#endif // CONTAINERFORMAT_H
