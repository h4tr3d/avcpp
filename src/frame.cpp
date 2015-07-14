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

VideoFrame2::VideoFrame2(AVPixelFormat pixelFormat, int width, int height, int align)
{
    m_raw->format = pixelFormat;
    m_raw->width  = width;
    m_raw->height = height;
    av_frame_get_buffer(m_raw, align);
}

VideoFrame2::VideoFrame2(const uint8_t *data, size_t size, AVPixelFormat pixelFormat, int width, int height, int align) throw(std::length_error)
    : VideoFrame2(pixelFormat, width, height, align)
{
    size_t calcSize = av_image_get_buffer_size(pixelFormat, width, height, align);
    if (calcSize != size)
        throw length_error("Data size and required buffer for this format/width/height/align not equal");

    uint8_t *buf[4];
    int      linesize[4];
    av_image_fill_arrays(buf, linesize, data, pixelFormat, width, height, align);

    // copy data
    for (size_t i = 0; i < 4 && buf[i]; ++i) {
        std::copy(buf[i], buf[i]+linesize[i], m_raw->data[i]);
    }
}

VideoFrame2::VideoFrame2(const VideoFrame2 &other)
    : Frame2<VideoFrame2>(other)
{
}

VideoFrame2::VideoFrame2(VideoFrame2 &&other)
    : Frame2<VideoFrame2>(move(other))
{
}

VideoFrame2 &VideoFrame2::operator=(const VideoFrame2 &rhs)
{
    return assignOperator(rhs);
}

VideoFrame2 &VideoFrame2::operator=(VideoFrame2 &&rhs)
{
    return moveOperator(move(rhs));
}

AVPixelFormat VideoFrame2::pixelFormat() const
{
    return static_cast<AVPixelFormat>(RAW_GET(format, AV_PIX_FMT_NONE));
}

int VideoFrame2::width() const
{
    return RAW_GET(width, 0);
}

int VideoFrame2::height() const
{
    return RAW_GET(height, 0);
}

bool VideoFrame2::isKeyFrame() const
{
    return RAW_GET(key_frame, false);
}

void VideoFrame2::setKeyFrame(bool isKey)
{
    RAW_SET(key_frame, isKey);
}

int VideoFrame2::quality() const
{
    return RAW_GET(quality, 0);
}

void VideoFrame2::setQuality(int quality)
{
    RAW_SET(quality, quality);
}

AVPictureType VideoFrame2::pictureType() const
{
    return RAW_GET(pict_type, AV_PICTURE_TYPE_NONE);
}

void VideoFrame2::setPictureType(AVPictureType type)
{
    RAW_SET(pict_type, type);
}


AudioSamples2::AudioSamples2(AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align)
{
    init(sampleFormat, samplesCount, channelLayout, sampleRate, align);
}

int AudioSamples2::init(AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align)
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

    av_frame_set_sample_rate(m_raw, sampleRate);
    av_frame_set_channel_layout(m_raw, channelLayout);

    av_frame_get_buffer(m_raw, align);
    return 0;
}

AudioSamples2::AudioSamples2(const uint8_t *data, size_t size, AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align) throw(std::length_error)
    : AudioSamples2(sampleFormat, samplesCount, channelLayout, sampleRate, align)
{
    const auto channels = av_get_channel_layout_nb_channels(channelLayout);
    size_t calcSize = av_samples_get_buffer_size(nullptr, channels, samplesCount, sampleFormat, align);
    if (calcSize > size)
        throw length_error("Data size and required buffer for this format/nb_samples/nb_channels/align not equal");

    uint8_t *buf[channels];
    int      linesize[channels];
    av_samples_fill_arrays(buf, linesize, data, channels, samplesCount, sampleFormat, align);

    // copy data
    for (size_t i = 0; i < size_t(channels) && i < size_t(AV_NUM_DATA_POINTERS); ++i) {
        std::copy(buf[i], buf[i]+linesize[i], m_raw->data[i]);
    }
}

AudioSamples2::AudioSamples2(const AudioSamples2 &other)
    : Frame2<AudioSamples2>(other)
{
}

AudioSamples2::AudioSamples2(AudioSamples2 &&other)
    : Frame2<AudioSamples2>(move(other))
{
}

AudioSamples2 &AudioSamples2::operator=(const AudioSamples2 &rhs)
{
    return assignOperator(rhs);
}

AudioSamples2 &AudioSamples2::operator=(AudioSamples2 &&rhs)
{
    return moveOperator(move(rhs));
}

AVSampleFormat AudioSamples2::sampleFormat() const
{
    return static_cast<AVSampleFormat>(RAW_GET(format, AV_SAMPLE_FMT_NONE));
}

int AudioSamples2::samplesCount() const
{
    return RAW_GET(nb_samples, 0);
}

int AudioSamples2::channelsCount() const
{
    return m_raw ? av_frame_get_channels(m_raw) : 0;
}

int64_t AudioSamples2::channelsLayout() const
{
    return m_raw ? av_frame_get_channel_layout(m_raw) : 0;
}

int AudioSamples2::sampleRate() const
{
    return m_raw ? av_frame_get_sample_rate(m_raw) : 0;
}

uint AudioSamples2::sampleBitDepth() const
{
    return m_raw ? (av_get_bytes_per_sample(static_cast<AVSampleFormat>(m_raw->format))) << 3 : 0;
}

string AudioSamples2::channelsLayoutString() const
{
    if (!m_raw)
        return "";
    char buf[128] = {0};
    av_get_channel_layout_string(buf,
                                 sizeof(buf),
                                 av_frame_get_channels(m_raw),
                                 av_frame_get_channel_layout(m_raw));
    return string(buf);
}

void AudioSamples2::setFakePts(int64_t pts)
{
    m_fakePts = pts;
}

int64_t AudioSamples2::fakePts() const
{
    return m_fakePts;
}


} // ::av
