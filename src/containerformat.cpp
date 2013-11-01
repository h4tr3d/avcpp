#include "containerformat.h"

namespace av
{

ContainerFormat::ContainerFormat()
    : inputFormat(0),
      outputFormat(0)
{
}

ContainerFormat::ContainerFormat(AVInputFormat *inputFormat, AVOutputFormat *outputFormat)
    : inputFormat(inputFormat),
      outputFormat(outputFormat)
{
}

ContainerFormat::ContainerFormat(const ContainerFormat &format)
{
    inputFormat = format.getInputFormat();
    outputFormat = format.getOutputFormat();
}

ContainerFormat::~ContainerFormat()
{
    inputFormat = 0;
    outputFormat = 0;
}

ContainerFormat &ContainerFormat::operator =(const ContainerFormat &format)
{
    inputFormat = format.getInputFormat();
    outputFormat = format.getOutputFormat();

    return *this;
}

AVInputFormat *ContainerFormat::getInputFormat() const
{
    return inputFormat;
}

AVOutputFormat *ContainerFormat::getOutputFormat() const
{
    return outputFormat;
}

void ContainerFormat::setInputFormat(AVInputFormat *format)
{
    inputFormat = format;
}

void ContainerFormat::setOutputFormat(AVOutputFormat *format)
{
    outputFormat = format;
}

bool ContainerFormat::setInputFormat(const char *name)
{
    if (name)
    {
        inputFormat = av_find_input_format(name);
    }
    else
    {
        inputFormat = 0;
    }

    return inputFormat;
}

bool ContainerFormat::setOutputFormat(const char *name, const char *url, const char *mime)
{
    if (name || url || mime)
    {
        outputFormat = av_guess_format(name, url, mime);
    }
    else
    {
        outputFormat = 0;
    }

    return outputFormat;
}

const char *ContainerFormat::getInputFormatName()
{
    return (inputFormat ? inputFormat->name : 0);
}

const char *ContainerFormat::getInputFormatLongName()
{
    return (inputFormat ? inputFormat->long_name : 0);
}

const char *ContainerFormat::getOutputFormatName()
{
    return (outputFormat ? outputFormat->name : 0);
}

const char *ContainerFormat::getOutputFormatLongName()
{
    return (outputFormat ? outputFormat->long_name : 0);
}

const char *ContainerFormat::getOutputFormatMimeType()
{
    return (outputFormat ? outputFormat->mime_type : 0);
}

bool ContainerFormat::isInput()
{
    return inputFormat;
}

bool ContainerFormat::isOutput()
{
    return outputFormat;
}

int32_t ContainerFormat::getInputFlags()
{
    return (inputFormat ? inputFormat->flags : 0);
}

void ContainerFormat::setInputFlags(int32_t flags)
{
    if (inputFormat)
        inputFormat->flags = flags;
}

void ContainerFormat::addInputFlags(int32_t flags)
{
    if (inputFormat)
        inputFormat->flags |= flags;
}

void ContainerFormat::clearInputFlags(int32_t flags)
{
    if (inputFormat)
        inputFormat->flags &= ~(flags);
}

int32_t ContainerFormat::getOutputFlags()
{
    return (outputFormat ? outputFormat->flags : 0);
}

void ContainerFormat::setOutputFlags(int32_t flags)
{
    if (outputFormat)
        outputFormat->flags = flags;
}

void ContainerFormat::addOutputFlags(int32_t flags)
{
    if (outputFormat)
        outputFormat->flags |= flags;
}

void ContainerFormat::clearOutputFlags(int32_t flags)
{
    if (outputFormat)
        outputFormat->flags &= ~flags;
}

AVCodecID ContainerFormat::getOutputDefaultAudioCodec()
{
    return (outputFormat ? outputFormat->audio_codec : CODEC_ID_NONE);
}

AVCodecID ContainerFormat::getOutputDefaultVideoCodec()
{
    return (outputFormat ? outputFormat->video_codec : CODEC_ID_NONE);
}

bool ContainerFormat::isCodecSupportedForOutput(AVCodecID codecId)
{
    if (outputFormat)
        return (avformat_query_codec(outputFormat, codecId, FF_COMPLIANCE_NORMAL) > 0);
    return false;
}

boost::shared_ptr<ContainerFormat> ContainerFormat::guessOutputFormat(const char *name, const char *url, const char *mime)
{
    boost::shared_ptr<ContainerFormat> result;
    AVOutputFormat *format = av_guess_format(name, url, mime);
    if (!format)
    {
        return result;
    }

    result.reset(new ContainerFormat(0, format));

    return result;
}




} // ::av
