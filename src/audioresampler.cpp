#include <cstdio>

#include "audioresampler.h"
#include "avlog.h"

using namespace std;

namespace av {

AudioResampler::AudioResampler()
{
}

AudioResampler::AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                               int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat)
    : AudioResampler()
{
    init(dstChannelsLayout, dstRate, dstFormat, srcChannelsLayout, srcRate, srcFormat);
}

AudioResampler::~AudioResampler()
{
    if (m_raw)
    {
        swr_free(&m_raw);
    }
}

AudioResampler::AudioResampler(AudioResampler && other)
    : AudioResampler()
{
    swap(other);
}

AudioResampler& AudioResampler::operator=(AudioResampler && rhs)
{
    if (this != &rhs) {
        swap(rhs);
        AudioResampler().swap(rhs);
    }
    return *this;
}

void AudioResampler::swap(AudioResampler & other)
{
    using std::swap;
#define SWAP(x) swap(x, other.x);
    SWAP(m_dstChannelsLayout);
    SWAP(m_dstRate);
    SWAP(m_dstFormat);
    SWAP(m_srcChannelsLayout);
    SWAP(m_srcRate);
    SWAP(m_srcFormat);
    SWAP(m_streamIndex);
    SWAP(m_prevPts);
    SWAP(m_nextPts);
#undef SWAP
}



int32_t AudioResampler::pop(AudioSamples2 & dst, bool getAllAvail)
{
    if (!m_raw) {
        fflog(AV_LOG_ERROR, "SwrContext does not inited\n");
        return -1;
    }

    // TODO: add checks for input and output parameters

    auto result = swr_get_delay(m_raw, m_dstRate);
    //clog << "  delay [pop]: " << result << endl;

    // Need more data
    if (result < dst.samplesCount() && getAllAvail == false) {
        return 0;
    }

    auto st = swr_convert_frame(m_raw, dst.raw(), nullptr);

    if (st == 0) {
        dst.setTimeBase(Rational(1, m_dstRate));
        dst.setStreamIndex(m_streamIndex);
        dst.setComplete(true);

        // Ugly PTS handling. More clean one can be done by user code
        if (m_nextPts == AV_NOPTS_VALUE) {
            m_nextPts = 0;
        }
        dst.setPts(m_nextPts);
        m_nextPts = dst.pts() + dst.samplesCount();
    }

    //result = swr_get_delay(m_raw, m_dstRate);
    //clog << "  delay [pop]: " << result << endl;

    return st == 0 ? dst.samplesCount() : st;
}

int32_t AudioResampler::push(const AudioSamples2 & src)
{
    if (!m_raw) {
        fflog(AV_LOG_ERROR, "SwrContext does not inited\n");
        return -1;
    }

    // TODO: add checks for input and output parameters

    auto st = swr_convert_frame(m_raw, nullptr, src.raw());
    if (st == 0) {
        m_streamIndex = src.streamIndex();

        if (m_prevPts > src.pts()) // Reset case
            m_nextPts = AV_NOPTS_VALUE;
        m_prevPts     = src.pts();
    }

    auto result = swr_get_delay(m_raw, m_dstRate);
    clog << "  delay [push]: " << result << endl;

    return st;
}


bool AudioResampler::isValid() const
{
    return !!m_raw;
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

    if (m_raw == nullptr) {
        m_raw = swr_alloc();
        if (m_raw == nullptr) {
            fflog(AV_LOG_FATAL, "Can't alloc SwrContext\n");
            return false;
        }
    }

    /* set options */
    av_opt_set_int(m_raw,        "in_channel_layout",     srcChannelsLayout, 0);
    av_opt_set_int(m_raw,        "in_sample_rate",        srcRate,           0);
    av_opt_set_sample_fmt(m_raw, "in_sample_fmt",         srcFormat,         0);

    av_opt_set_int(m_raw,        "out_channel_layout",    dstChannelsLayout, 0);
    av_opt_set_int(m_raw,        "out_sample_rate",       dstRate,           0);
    av_opt_set_sample_fmt(m_raw, "out_sample_fmt",        dstFormat,         0);

    if (swr_init(m_raw) < 0)
    {
        fflog(AV_LOG_ERROR, "Can't initalize Audio Resample context\n");
        swr_free(&m_raw);
        return false;
    }

    this->m_dstChannelsLayout = dstChannelsLayout;
    this->m_dstRate           = dstRate;
    this->m_dstFormat         = dstFormat;
    this->m_srcChannelsLayout = srcChannelsLayout;
    this->m_srcRate           = srcRate;
    this->m_srcFormat         = srcFormat;

    return true;
}


} // namespace av
