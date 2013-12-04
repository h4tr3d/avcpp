#include <cstdio>

#include "audioresampler.h"

namespace av {

AudioResampler::AudioResampler()
    : context(0)
{
}

AudioResampler::AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                               int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat)
    : context(0)
{
    init(dstChannelsLayout, dstRate, dstFormat, srcChannelsLayout, srcRate, srcFormat);
}

AudioResampler::~AudioResampler()
{
    if (context)
    {
        swr_free(&context);
    }
}

int32_t AudioResampler::resample(const AudioSamplesPtr &dst, const AudioSamplesPtr &src, uint samplesCount)
{
    if (!context)
    {
        cerr << "AudioResampler context does not allocated" << endl;
        return -1;
    }

    if (dst == src)
    {
        cerr << "Destination and Source sampes is same" << endl;
        return -1;
    }

    if (!src)
    {
        cerr << "Null Source samples" << endl;
        return -1;
    }

    if (!dst)
    {
        cerr << "Null Destination samples" << endl;
        return -1;
    }

    if (!src->isComplete())
    {
        cerr << "Input samples does not complete" << endl;
        return -1;
    }

    if (src->getChannelsCount() != getSrcChannels())
    {
        cerr << "Input channels count mistmatch" << endl;
        return -1;
    }

    if (src->getSampleRate() != srcRate)
    {
        cerr << "Input sample rate mistmatch" << endl;
        return -1;
    }

    if (src->getSampleFormat() != srcFormat)
    {
        cerr << "Input sample format mistmatch" << endl;
        return -1;
    }

    if (dst->getChannelsCount() != getDstChannels())
    {
        cerr << "Output channels count mistmatch" << endl;
        return -1;
    }

    if (dst->getSampleRate() != dstRate)
    {
        cerr << "Output sample rate mistmatch" << endl;
        return -1;
    }

    if (dst->getSampleFormat() != dstFormat)
    {
        cerr << "Output sample format mistmatch" << endl;
        return -1;
    }

    const uint8_t **in = const_cast<const uint8_t**>(src->getAVFrame()->extended_data);

    int count = swr_convert(context,
                            dst->getAVFrame()->extended_data, dst->getSamplesCount() / dst->getChannelsCount(),
                            in,                               src->getSamplesCount() / src->getChannelsCount());


    dst->setTimeBase(src->getTimeBase());
    dst->setStreamIndex(src->getStreamIndex());
    dst->setPts(src->getPts());
    dst->setComplete(cout >= 0);

    return count;
}

bool AudioResampler::isValid() const
{
    return !!context;
}

bool AudioResampler::init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                          int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat)
{
    if (dstChannelsLayout <= 0)
    {
        return false;
    }

    if (dstRate <= 0)
    {
        return false;
    }

    if (dstFormat == -1)
    {
        return false;
    }

    if (srcChannelsLayout <= 0)
    {
        return false;
    }

    if (srcRate <= 0)
    {
        return false;
    }

    if (srcFormat == -1)
    {
        return false;
    }

    if (context)
    {
        swr_free(&context);
    }


    context = swr_alloc_set_opts(0,
                                 dstChannelsLayout, dstFormat, dstRate,
                                 srcChannelsLayout, srcFormat, srcRate,
                                 0, 0);

    if (!context)
    {
        cerr << "Can't allocate Audio Resample context" << endl;
        return false;
    }

    if (swr_init(context) < 0)
    {
        cerr << "Can't initalize Audio Resample context" << endl;
        swr_free(&context);
        return false;
    }

    this->dstChannelsLayout = dstChannelsLayout;
    this->dstRate           = dstRate;
    this->dstFormat         = dstFormat;
    this->srcChannelsLayout = srcChannelsLayout;
    this->srcRate           = srcRate;
    this->srcFormat         = srcFormat;

    return true;
}


} // namespace av
