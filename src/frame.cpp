#include "frame.h"

using namespace std;

extern "C" {
#include <libavutil/version.h>
#include <libavutil/imgutils.h>
#include <libavutil/avassert.h>
}


namespace {

#if LIBAVUTIL_VERSION_INT <= AV_VERSION_INT(52,48,101)

#define CHECK_CHANNELS_CONSISTENCY(frame) \
    av_assert2(!(frame)->channel_layout || \
               (frame)->channels == \
               av_get_channel_layout_nb_channels((frame)->channel_layout))

int frame_copy_video(AVFrame *dst, const AVFrame *src)
{
    const uint8_t *src_data[4];
    int i, planes;

    if (dst->width  < src->width ||
        dst->height < src->height)
        return AVERROR(EINVAL);

    planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(dst->format));
    for (i = 0; i < planes; i++)
        if (!dst->data[i] || !src->data[i])
            return AVERROR(EINVAL);

    memcpy(src_data, src->data, sizeof(src_data));
    av_image_copy(dst->data, dst->linesize,
                  src_data, src->linesize,
                  static_cast<AVPixelFormat>(dst->format), src->width, src->height);

    return 0;
}

int frame_copy_audio(AVFrame *dst, const AVFrame *src)
{
    int planar   = av_sample_fmt_is_planar(static_cast<AVSampleFormat>(dst->format));
    int channels = dst->channels;
    int planes   = planar ? channels : 1;
    int i;

    if (dst->nb_samples     != src->nb_samples ||
        dst->channels       != src->channels ||
        dst->channel_layout != src->channel_layout)
        return AVERROR(EINVAL);

    CHECK_CHANNELS_CONSISTENCY(src);

    for (i = 0; i < planes; i++)
        if (!dst->extended_data[i] || !src->extended_data[i])
            return AVERROR(EINVAL);

    av_samples_copy(dst->extended_data, src->extended_data, 0, 0,
                    dst->nb_samples, channels, static_cast<AVSampleFormat>(dst->format));

    return 0;
}

int av_frame_copy(AVFrame *dst, const AVFrame *src)
{
    if (dst->format != src->format || dst->format < 0)
        return AVERROR(EINVAL);

    if (dst->width > 0 && dst->height > 0)
        return frame_copy_video(dst, src);
    else if (dst->nb_samples > 0 && dst->channel_layout)
        return frame_copy_audio(dst, src);

    return AVERROR(EINVAL);
}

#undef CHECK_CHANNELS_CONSISTENCY

#endif // LIBAVUTIL_VERSION_INT <= 53.5.0 (ffmpeg 2.2)

} // anonymous namespace

namespace av
{

VideoFrame::VideoFrame(PixelFormat pixelFormat, int width, int height, int align)
{
    m_raw->format = pixelFormat;
    m_raw->width  = width;
    m_raw->height = height;
    av_frame_get_buffer(m_raw, align);
}

VideoFrame::VideoFrame(const uint8_t *data, size_t size, PixelFormat pixelFormat, int width, int height, int align)
    : VideoFrame(pixelFormat, width, height, align)
{
    size_t calcSize = av_image_get_buffer_size(pixelFormat, width, height, align);
    if (calcSize != size)
        throw length_error("Data size and required buffer for this format/width/height/align not equal");

    uint8_t *src_buf[4];
    int      src_linesize[4];
    av_image_fill_arrays(src_buf, src_linesize, data, pixelFormat, width, height, align);

    // copy data
    av_image_copy(m_raw->data, m_raw->linesize,
                  const_cast<const uint8_t**>(src_buf), const_cast<const int*>(src_linesize),
                  pixelFormat, width, height);
}

VideoFrame::VideoFrame(const VideoFrame &other)
    : Frame<VideoFrame>(other)
{
}

VideoFrame::VideoFrame(VideoFrame &&other)
    : Frame<VideoFrame>(move(other))
{
}

VideoFrame &VideoFrame::operator=(const VideoFrame &rhs)
{
    return assignOperator(rhs);
}

VideoFrame &VideoFrame::operator=(VideoFrame &&rhs)
{
    return moveOperator(move(rhs));
}

PixelFormat VideoFrame::pixelFormat() const
{
    return static_cast<AVPixelFormat>(RAW_GET(format, AV_PIX_FMT_NONE));
}

int VideoFrame::width() const
{
    return RAW_GET(width, 0);
}

int VideoFrame::height() const
{
    return RAW_GET(height, 0);
}

bool VideoFrame::isKeyFrame() const
{
    return RAW_GET(key_frame, false);
}

void VideoFrame::setKeyFrame(bool isKey)
{
    RAW_SET(key_frame, isKey);
}

int VideoFrame::quality() const
{
    return RAW_GET(quality, 0);
}

void VideoFrame::setQuality(int quality)
{
    RAW_SET(quality, quality);
}

AVPictureType VideoFrame::pictureType() const
{
    return RAW_GET(pict_type, AV_PICTURE_TYPE_NONE);
}

void VideoFrame::setPictureType(AVPictureType type)
{
    RAW_SET(pict_type, type);
}

Rational VideoFrame::sampleAspectRatio() const
{
    return RAW_GET(sample_aspect_ratio, AVRational());
}

void VideoFrame::setSampleAspectRatio(const Rational& sampleAspectRatio)
{
    RAW_SET(sample_aspect_ratio, sampleAspectRatio);
}

size_t VideoFrame::bufferSize(int align, OptionalErrorCode ec) const
{
    clear_if(ec);
    auto sz = av_image_get_buffer_size(static_cast<AVPixelFormat>(m_raw->format), m_raw->width, m_raw->height, align);
    if (sz < 0) {
        throws_if(ec, sz, ffmpeg_category());
        return 0;
    }
    return static_cast<size_t>(sz);
}

bool VideoFrame::copyToBuffer(uint8_t *dst, size_t size, int align, OptionalErrorCode ec)
{
    clear_if(ec);
    auto sts = av_image_copy_to_buffer(dst, static_cast<int>(size), m_raw->data, m_raw->linesize, static_cast<AVPixelFormat>(m_raw->format), m_raw->width, m_raw->height, align);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return false;
    }
    return true;
}

bool VideoFrame::copyToBuffer(std::vector<uint8_t> &dst, int align, OptionalErrorCode ec)
{
    auto sz = bufferSize(align, ec);
    if (ec && *ec)
        return false;
    dst.resize(sz);
    return copyToBuffer(dst.data(), dst.size(), align, ec);
}


AudioSamples::AudioSamples(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align)
{
    init(sampleFormat, samplesCount, channelLayout, sampleRate, align);
}

int AudioSamples::init(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align)
{
    if (!m_raw) {
        m_raw = av_frame_alloc();
        m_raw->opaque = this;
    }

    if (m_raw->data[0]) {
        av_frame_free(&m_raw);
    }

    m_raw->format      = sampleFormat;
    m_raw->nb_samples  = samplesCount;

    av::frame::set_sample_rate(m_raw, sampleRate);
    av::frame::set_channel_layout(m_raw, channelLayout);

    av_frame_get_buffer(m_raw, align);
    return 0;
}

AudioSamples::AudioSamples(const uint8_t *data, size_t size, SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align)
    : AudioSamples(sampleFormat, samplesCount, channelLayout, sampleRate, align)
{
    const auto channels = av_get_channel_layout_nb_channels(channelLayout);
    auto calcSize = sampleFormat.requiredBufferSize(channels, samplesCount, align);

    if (calcSize > size)
        throw length_error("Data size and required buffer for this format/nb_samples/nb_channels/align not equal");

    uint8_t *buf[AV_NUM_DATA_POINTERS];
    int      linesize[AV_NUM_DATA_POINTERS];
    SampleFormat::fillArrays(buf, linesize, data, channels, samplesCount, sampleFormat, align);

    // copy data
    for (size_t i = 0; i < size_t(channels) && i < size_t(AV_NUM_DATA_POINTERS); ++i) {
        std::copy(buf[i], buf[i]+linesize[i], m_raw->data[i]);
    }
}

AudioSamples::AudioSamples(const AudioSamples &other)
    : Frame<AudioSamples>(other)
{
}

AudioSamples::AudioSamples(AudioSamples &&other)
    : Frame<AudioSamples>(move(other))
{
}

AudioSamples &AudioSamples::operator=(const AudioSamples &rhs)
{
    return assignOperator(rhs);
}

AudioSamples &AudioSamples::operator=(AudioSamples &&rhs)
{
    return moveOperator(move(rhs));
}

SampleFormat AudioSamples::sampleFormat() const
{
    return static_cast<AVSampleFormat>(RAW_GET(format, AV_SAMPLE_FMT_NONE));
}

int AudioSamples::samplesCount() const
{
    return RAW_GET(nb_samples, 0);
}

int AudioSamples::channelsCount() const
{
    return m_raw ? av::frame::get_channels(m_raw) : 0;
}

uint64_t AudioSamples::channelsLayout() const
{
    return m_raw ? av::frame::get_channel_layout(m_raw) : 0;
}

int AudioSamples::sampleRate() const
{
    return m_raw ? av::frame::get_sample_rate(m_raw) : 0;
}

size_t AudioSamples::sampleBitDepth(OptionalErrorCode ec) const
{
    return m_raw
            ? SampleFormat(static_cast<AVSampleFormat>(m_raw->format)).bitsPerSample(ec)
            : 0;
}

bool AudioSamples::isPlanar() const
{
    return m_raw ? av_sample_fmt_is_planar(static_cast<AVSampleFormat>(m_raw->format)) : false;
}

string AudioSamples::channelsLayoutString() const
{
    if (!m_raw)
        return "";
    char buf[128] = {0};
    av_get_channel_layout_string(buf,
                                 sizeof(buf),
                                 av::frame::get_channels(m_raw),
                                 av::frame::get_channel_layout(m_raw));
    return string(buf);
}

} // ::av
