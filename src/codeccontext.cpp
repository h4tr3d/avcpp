#include "avlog.h"
#include "avutils.h"

#include "codeccontext.h"

using namespace std;

namespace av {

CodecContext::CodecContext(const Stream2 &st)
    : m_stream(st)
{
    m_raw = st.raw()->codec;

    Codec codec;
    if (st.direction() == DECODING)
    {
        codec = findDecodingCodec(m_raw->codec_id);
    }
    else
    {
        codec = findEncodingCodec(m_raw->codec_id);
    }

    if (!codec.isNull())
        setCodec(codec);

    m_direction = st.direction();
}

void CodecContext::setCodec(const Codec &codec)
{
    if (!m_raw || !m_stream.isValid())
    {
        null_log(AV_LOG_WARNING, "Parent stream is not valid. Probably it or FormatContext destrayed\n");
        return;
    }

    if (codec.isNull())
    {
        ptr_log(AV_LOG_WARNING, "Try to set null codec\n");
        return;
    }

    if (!m_raw)
    {
        ptr_log(AV_LOG_WARNING, "Codec context does not allocated\n");
        return;
    }

    if (m_codec.raw() == codec.raw())
    {
        return;
    }

    if (m_direction == ENCODING && codec.canEncode() == false)
    {
        ptr_log(AV_LOG_WARNING, "Encoding context, but codec does not support encoding\n");
        return;
    }

    if (m_direction == DECODING && codec.canDecode() == false)
    {
        ptr_log(AV_LOG_WARNING, "Decoding context, but codec does not support decoding\n");
        return;
    }

    m_codec = codec;

    m_raw->codec_id   = codec.getId();
    m_raw->codec_type = codec.type();
    m_raw->codec      = codec.raw();

    if (codec.raw()->pix_fmts != 0)
    {
        m_raw->pix_fmt = *(codec.raw()->pix_fmts); // assign default value
    }

    if (codec.raw()->sample_fmts != 0)
    {
        m_raw->sample_fmt = *(codec.raw()->sample_fmts);
    }
}

bool CodecContext::open()
{
    if (m_isOpened || !isValid())
        return false;

    // HACK: set threads to 1
    AVDictionary *opts = 0;
    av_dict_set(&opts, "threads", "1", 0);
    ////////////////////////////////////////////////////////////////////////////////////////////

    bool ret = true;
    int stat = avcodec_open2(m_raw, m_codec.raw(), &opts);
    if (stat < 0)
        ret = false;
    else
        m_isOpened = true;

    av_dict_free(&opts);

    return ret;
}

bool CodecContext::close()
{
    if (m_isOpened && isValid())
    {
        avcodec_close(m_raw);
        m_isOpened = false;
        return true;
    }
    return false;
}

bool CodecContext::isValid() const
{
    return (m_raw && !m_codec.isNull() && m_stream.isValid());
}

bool CodecContext::copyContextFrom(const CodecContext &other)
{
    if (!isValid()) {
        null_log(AV_LOG_WARNING, "Invalid target context\n");
        return false;
    }
    if (!other.isValid()) {
        null_log(AV_LOG_WARNING, "Invalid source context\n");
        return false;
    }
    if (isOpened()) {
        ptr_log(AV_LOG_WARNING, "Try to copy context to opened target context\n");
        return false;
    }

    int stat = avcodec_copy_context(m_raw, other.m_raw);
    m_raw->codec_tag = 0;

    return stat < 0 ? false : true;
}

Rational CodecContext::timeBase() const
{
    return RAW_GET2(isValid(), time_base, Rational());
}

void CodecContext::setTimeBase(const Rational &value)
{
    RAW_SET2(isValid(), time_base, value.getValue());
}

const Stream2 &CodecContext::stream() const
{
    return m_stream;
}

AVMediaType CodecContext::codecType() const
{
    if (isValid())
    {
        if (m_raw->codec && m_raw->codec_type != m_raw->codec->type)
            ptr_log(AV_LOG_ERROR, "Non-consistent AVCodecContext::codec_type and AVCodec::type\n");

        return m_raw->codec_type;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}

void CodecContext::setOption(const string &key, const string &val, int flags)
{
    if (isValid())
    {
        av_opt_set(m_raw->priv_data, key.c_str(), val.c_str(), flags);
    }
}

bool CodecContext::isAudio() const
{
    return codecType() == AVMEDIA_TYPE_VIDEO;
}

bool CodecContext::isVideo() const
{
    return codecType() == AVMEDIA_TYPE_AUDIO;
}

int CodecContext::frameSize() const
{
    return RAW_GET2(isValid(), frame_size, 0);
}

int CodecContext::frameNumber() const
{
    return RAW_GET2(isValid(), frame_number, 0);
}

int CodecContext::width() const
{
    return RAW_GET2(isValid(), width, 0);
}

int CodecContext::height() const
{
    return RAW_GET2(isValid(), height, 0);
}

int CodecContext::codedWidth() const
{
    return RAW_GET2(isValid(), coded_width, 0);
}

int CodecContext::codedHeight() const
{
    return RAW_GET2(isValid(), coded_height, 0);
}

AVPixelFormat CodecContext::pixelFormat() const
{
    return RAW_GET2(isValid(), pix_fmt, AV_PIX_FMT_NONE);
}

Rational CodecContext::frameRate()
{
    return m_stream.frameRate();
}

int32_t CodecContext::bitRate() const
{
    return RAW_GET2(isValid(), bit_rate, 0);
}

std::pair<int, int> CodecContext::bitRateRange() const
{
    if (isValid())
        return make_pair(m_raw->rc_min_rate, m_raw->rc_max_rate);
    else
        return make_pair(0, 0);
}

int32_t CodecContext::globalQuality()
{
    return RAW_GET2(isValid(), global_quality, FF_LAMBDA_MAX);
}

int32_t CodecContext::gopSize()
{
    return RAW_GET2(isValid(), gop_size, 0);
}

int CodecContext::bitRateTolerance() const
{
    return RAW_GET2(isValid(), bit_rate_tolerance, 0);
}

int CodecContext::strict() const
{
    return RAW_GET2(isValid(), strict_std_compliance, 0);
}

int CodecContext::maxBFrames() const
{
    return RAW_GET2(isValid(), max_b_frames, 0);
}

void CodecContext::setWidth(int w)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->width       = w;
        m_raw->coded_width = w;
    }
}

void CodecContext::setHeight(int h)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->height       = h;
        m_raw->coded_height = h;
    }
}

void CodecContext::setCodedWidth(int w)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, coded_width, w);
}

void CodecContext::setCodedHeight(int h)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, coded_height, h);
}

void CodecContext::setPixelFormat(AVPixelFormat pixelFormat)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, pix_fmt, pixelFormat);
}

void CodecContext::setFrameRate(const Rational &frameRate)
{
    warnIfNotVideo();
    m_stream.setFrameRate(frameRate);
}

void CodecContext::setBitRate(int32_t bitRate)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, bit_rate, bitRate);
}

void CodecContext::setBitRateRange(const std::pair<int, int> &bitRateRange)
{
    warnIfNotVideo();
    if (isValid() && !m_isOpened)
    {
        m_raw->rc_min_rate = get<0>(bitRateRange);
        m_raw->rc_max_rate = get<1>(bitRateRange);
    }
}

void CodecContext::setGlobalQuality(int32_t quality)
{
    warnIfNotVideo();
    if (quality < 0 || quality > FF_LAMBDA_MAX)
        quality = FF_LAMBDA_MAX;

    RAW_SET2(isValid() && !m_isOpened, global_quality, quality);
}

void CodecContext::setGopSize(int32_t size)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, gop_size, size);
}

void CodecContext::setBitRateTolerance(int bitRateTolerance)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, bit_rate_tolerance, bitRateTolerance);
}

void CodecContext::setStrict(int strict)
{
    warnIfNotVideo();
    if (strict < FF_COMPLIANCE_EXPERIMENTAL)
        strict = FF_COMPLIANCE_EXPERIMENTAL;
    else if (strict > FF_COMPLIANCE_VERY_STRICT)
        strict = FF_COMPLIANCE_VERY_STRICT;

    RAW_SET2(isValid() && !m_isOpened, strict_std_compliance, strict);
}

void CodecContext::setMaxBFrames(int maxBFrames)
{
    warnIfNotVideo();
    RAW_SET2(isValid() && !m_isOpened, max_b_frames, maxBFrames);
}

int CodecContext::sampleRate() const
{
    warnIfNotVideo();
    return RAW_GET2(isValid(), sample_rate, 0);
}

int CodecContext::channels() const
{
    if (!isValid())
        return 0;

    if (m_raw->channels)
        return m_raw->channels;

    if (m_raw->channel_layout)
        return av_get_channel_layout_nb_channels(m_raw->channel_layout);

    return 0;
}

AVSampleFormat CodecContext::sampleFormat() const
{
    return RAW_GET2(isValid(), sample_fmt, AV_SAMPLE_FMT_NONE);
}

uint64_t CodecContext::channelLayout() const
{
    if (!isValid())
        return 0;

    if (m_raw->channel_layout)
        return m_raw->channel_layout;

    if (m_raw->channels)
        return av_get_default_channel_layout(m_raw->channels);

    return 0;
}

void CodecContext::setSampleRate(int sampleRate)
{
    warnIfNotAudio();
    if (!isValid() || m_isOpened)
        return;
    int sr = guessValue(sampleRate, m_codec.raw()->supported_samplerates, EqualComparator<int>(0));
    if (sr != sampleRate)
        ptr_log(AV_LOG_INFO, "Guess sample rate %d instead unsupported %d\n", sr, sampleRate);
    if (sr > 0)
        m_raw->sample_rate = sr;
}

void CodecContext::setChannels(int channels)
{
    if (!isValid() || channels <= 0 || m_isOpened)
        return;
    m_raw->channels = channels;
    if (m_raw->channel_layout != 0 ||
        av_get_channel_layout_nb_channels(m_raw->channel_layout) != channels) {
        m_raw->channel_layout = av_get_default_channel_layout(channels);
    }
}

void CodecContext::setSampleFormat(AVSampleFormat sampleFormat)
{
    warnIfNotAudio();
    RAW_SET2(isValid() && !m_isOpened, sample_fmt, sampleFormat);
}

void CodecContext::setChannelLayout(uint64_t layout)
{
    warnIfNotAudio();
    if (!isValid() || m_isOpened || layout == 0)
        return;

    m_raw->channel_layout = layout;

    // Make channels and channel_layout sync
    if (m_raw->channels == 0 ||
        av_get_default_channel_layout(m_raw->channels) != layout)
    {
        m_raw->channels = av_get_channel_layout_nb_channels(layout);
    }
}

void CodecContext::setFlags(int flags)
{
    RAW_SET2(isValid() && !m_isOpened, flags, flags);
}

void CodecContext::addFlags(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags |= flags;
}

void CodecContext::clearFlags(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags &= ~flags;
}

int CodecContext::getFlags()
{
    return RAW_GET2(isValid() && !m_isOpened, flags, 0);
}

bool CodecContext::isFlags(int flags)
{
    if (isValid() && !m_isOpened)
        return (m_raw->flags & flags);
    return false;
}

//
void CodecContext::setFlags2(int flags)
{
    RAW_SET2(isValid() && !m_isOpened, flags2, flags);
}

void CodecContext::addFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags2 |= flags;
}

void CodecContext::clearFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        m_raw->flags2 &= ~flags;
}

int CodecContext::getFlags2()
{
    return RAW_GET2(isValid() && !m_isOpened, flags2, 0);
}

bool CodecContext::isFlags2(int flags)
{
    if (isValid() && !m_isOpened)
        return (m_raw->flags2 & flags);
    return false;
}

bool CodecContext::isValidForEncode()
{
    if (!isValid())
    {
        null_log(AV_LOG_WARNING,
                 "Not valid context: codec_context=%p, stream_valid=%d, codec=%p\n",
                 m_raw,
                 m_stream.isValid(),
                 m_codec.raw());
        return false;
    }

    if (!m_isOpened)
    {
        ptr_log(AV_LOG_WARNING, "You must open coder before encoding\n");
        return false;
    }

    if (m_direction == DECODING)
    {
        ptr_log(AV_LOG_WARNING, "Decoding coder does not valid for encoding\n");
        return false;
    }

    if (!m_codec.canEncode())
    {
        ptr_log(AV_LOG_WARNING, "Codec can't be used for Encode\n");
        return false;
    }

    return true;
}
//

void CodecContext::warnIfNotVideo() const
{
    if (!isValid())
        return;
    if (!isVideo())
        ptr_log(AV_LOG_WARNING, "Try to set parameter for video but context not for video\n");
}

void CodecContext::warnIfNotAudio() const
{
    if (!isValid())
        return;
    if (!isAudio())
        ptr_log(AV_LOG_WARNING, "Try to set parameter for audio but context not for audio\n");
}



} // namespace av
