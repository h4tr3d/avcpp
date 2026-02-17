#include "frame.h"

using namespace std;

extern "C" {
#include <libavutil/version.h>
#include <libavutil/imgutils.h>
#include <libavutil/avassert.h>
}


namespace {

#if AVCPP_AVUTIL_VERSION_INT <= AV_VERSION_INT(52,48,101)

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

#endif // AVCPP_AVUTIL_VERSION_INT <= 53.5.0 (ffmpeg 2.2)

} // anonymous namespace

namespace av
{

namespace frame {

namespace priv {
void channel_layout_copy(AVFrame &dst, const AVFrame &src);
}

int64_t get_best_effort_timestamp(const AVFrame* frame) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    return av_frame_get_best_effort_timestamp(frame);
#else
    return frame->best_effort_timestamp;
#endif
}

// Based on db6efa1815e217ed76f39aee8b15ee5c64698537
static inline uint64_t get_channel_layout(const AVFrame* frame) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    return static_cast<uint64_t>(av_frame_get_channel_layout(frame));
#elif AVCPP_API_NEW_CHANNEL_LAYOUT
    return frame->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ? frame->ch_layout.u.mask : 0;
#else
    return frame->channel_layout;
#endif
}

static inline void set_channel_layout(AVFrame* frame, uint64_t layout) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    av_frame_set_channel_layout(frame, static_cast<int64_t>(layout));
#elif AVCPP_API_NEW_CHANNEL_LAYOUT
    av_channel_layout_uninit(&frame->ch_layout);
    av_channel_layout_from_mask(&frame->ch_layout, layout);
#else
    frame->channel_layout = layout;
#endif
}


static inline int get_channels(const AVFrame* frame) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    return av_frame_get_channels(frame);
#elif AVCPP_API_NEW_CHANNEL_LAYOUT
    return frame->ch_layout.nb_channels;
#else
    return frame->channels;
#endif
}

static inline bool is_valid_channel_layout(const AVFrame *frame)
{
#if AVCPP_API_NEW_CHANNEL_LAYOUT
    return av_channel_layout_check(&frame->ch_layout);
#else
    return frame->channel_layout;
#endif
}

static inline int get_sample_rate(const AVFrame* frame) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    return av_frame_get_sample_rate(frame);
#else
    return frame->sample_rate;
#endif
}

static inline void set_sample_rate(AVFrame* frame, int sampleRate) {
#if AVCPP_AVUTIL_VERSION_MAJOR < 56 // < FFmpeg 4.0
    av_frame_set_sample_rate(frame, sampleRate);
#else
    frame->sample_rate = sampleRate;
#endif
}

} // ::frame


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
    : Frame<VideoFrame>(std::move(other))
{
}

VideoFrame &VideoFrame::operator=(const VideoFrame &rhs)
{
    return assignOperator(rhs);
}

VideoFrame &VideoFrame::operator=(VideoFrame &&rhs)
{
    return moveOperator(std::move(rhs));
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

template<typename LookupProc>
static PixelFormat adjustJpegPixelFormat(AVFrame *frame, LookupProc&& proc)
{
    return frame
               ? static_cast<AVPixelFormat>(std::exchange(frame->format, proc(static_cast<AVPixelFormat>(frame->format))))
               : AV_PIX_FMT_NONE;
}

PixelFormat VideoFrame::adjustFromJpegPixelFormat()
{
    return adjustJpegPixelFormat(m_raw, [](AVPixelFormat pixfmt) {
        switch (pixfmt) {
            case AV_PIX_FMT_YUVJ420P:
                return AV_PIX_FMT_YUV420P;
            case AV_PIX_FMT_YUVJ411P:
                return AV_PIX_FMT_YUV411P;
            case AV_PIX_FMT_YUVJ422P:
                return AV_PIX_FMT_YUV422P;
            case AV_PIX_FMT_YUVJ444P:
                return AV_PIX_FMT_YUV444P;
            case AV_PIX_FMT_YUVJ440P:
                return AV_PIX_FMT_YUV440P;
            default:
                return pixfmt;
        }
    });
}

PixelFormat VideoFrame::adjustToJpegPixelFormat()
{
    return adjustJpegPixelFormat(m_raw, [](AVPixelFormat pixfmt) {
        switch (pixfmt) {
            case AV_PIX_FMT_YUV420P:
                return AV_PIX_FMT_YUVJ420P;
            case AV_PIX_FMT_YUV411P:
                return AV_PIX_FMT_YUVJ411P;
            case AV_PIX_FMT_YUV422P:
                return AV_PIX_FMT_YUVJ422P;
            case AV_PIX_FMT_YUV444P:
                return AV_PIX_FMT_YUVJ444P;
            case AV_PIX_FMT_YUV440P:
                return AV_PIX_FMT_YUVJ440P;
            default:
                return pixfmt;
        }
    });
}

bool VideoFrame::isKeyFrame() const
{
#if AVCPP_API_FRAME_KEY
    return !!(RAW_GET(flags, 0) & AV_FRAME_FLAG_KEY);
#else
    return RAW_GET(key_frame, false);
#endif
}

void VideoFrame::setKeyFrame(bool isKey)
{
#if AVCPP_API_FRAME_KEY
    if (m_raw) {
        if (isKey) {
            m_raw->flags |= AV_FRAME_FLAG_KEY;
        } else {
            m_raw->flags &= ~(AV_FRAME_FLAG_KEY);
        }
    }
#else
    RAW_SET(key_frame, isKey);
#endif
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

namespace {
VideoFrame _wrap(const void *data, size_t size, PixelFormat pixelFormat, int width, int height, int align,
                 void (*deleter)(void *opaque, uint8_t *data), void *opaque)
{
    VideoFrame out;

    auto frame = out.raw();

    frame->format = pixelFormat;
    frame->width  = width;
    frame->height = height;

    // setup buffers
    frame->buf[0] = av_buffer_create(const_cast<uint8_t*>(static_cast<const uint8_t*>(data)), size, deleter, opaque, AV_BUFFER_FLAG_READONLY);

    av_image_fill_arrays(frame->data,
                         frame->linesize,
                         frame->buf[0]->data,
                         pixelFormat, width, height, align);

    frame->extended_data = frame->data;

    return out;
}
}

VideoFrame VideoFrame::wrap(const void *data, size_t size, PixelFormat pixelFormat, int width, int height, int align)
{
    return _wrap(data, size, pixelFormat, width, height, align, av::buffer::null_deleter, nullptr);
}

#if AVCPP_CXX_STANDARD >= 20
VideoFrame VideoFrame::wrap(std::span<const std::byte> data, PixelFormat pixelFormat, int width, int height, int align)
{
    return wrap(data.data(), data.size(), pixelFormat, width, height, align);
}

VideoFrame VideoFrame::wrap(std::span<const std::byte> data, void (*deleter)(void *opaque, uint8_t *data),
    void *opaque, PixelFormat pixelFormat, int width, int height, int align)
{
    return _wrap(data.data(), data.size(), pixelFormat, width, height, align, deleter, opaque);
}
#endif

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
    auto const channels = [](uint64_t mask) -> int {
#if AVCPP_API_NEW_CHANNEL_LAYOUT
        AVChannelLayout layout{};
        av_channel_layout_from_mask(&layout, mask);
        return layout.nb_channels;
#else
        return av_get_channel_layout_nb_channels(mask);
#endif
    } (channelLayout);

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
    : Frame<AudioSamples>(std::move(other))
{
}

AudioSamples &AudioSamples::operator=(const AudioSamples &rhs)
{
    return assignOperator(rhs);
}

AudioSamples &AudioSamples::operator=(AudioSamples &&rhs)
{
    return moveOperator(std::move(rhs));
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
#if AVCPP_API_NEW_CHANNEL_LAYOUT
    av_channel_layout_describe(&m_raw->ch_layout, buf, sizeof(buf));
#else
    av_get_channel_layout_string(buf,
                                 sizeof(buf),
                                 av::frame::get_channels(m_raw),
                                 av::frame::get_channel_layout(m_raw));
#endif
    return string(buf);
}

void frame::priv::channel_layout_copy(AVFrame &dst, const AVFrame &src)
{
#if AVCPP_API_NEW_CHANNEL_LAYOUT
    av_channel_layout_copy(&dst.ch_layout, &src.ch_layout);
#else
    dst.channel_layout = src.channel_layout;
    dst.channels       = src.channels;
#endif
}

FrameCommon::FrameCommon() {
    m_raw = av_frame_alloc();
    m_raw->opaque = this;
}

FrameCommon::~FrameCommon() {
    av_frame_free(&m_raw);
}

FrameCommon::FrameCommon(const AVFrame *frame) {
    if (frame) {
        m_raw = av_frame_alloc();
        m_raw->opaque = this;
        av_frame_ref(m_raw, frame);
    }
}

FrameCommon::FrameCommon(const FrameCommon &other) : FrameCommon(other.m_raw) {
    if (m_raw)
        copyInfoFrom(other);
}

FrameCommon::FrameCommon(FrameCommon &&other) : FrameCommon(nullptr) {
    if (other.m_raw) {
        m_raw = av_frame_alloc();
        m_raw->opaque = this;
        av_frame_move_ref(m_raw, other.m_raw);
        copyInfoFrom(other);
    }
}

bool FrameCommon::isReferenced() const {
    return m_raw && m_raw->buf[0];
}

int FrameCommon::refCount() const {
    if (m_raw && m_raw->buf[0])
        return av_buffer_get_ref_count(m_raw->buf[0]);
    else
        return 0;
}

AVFrame *FrameCommon::makeRef() const {
    return m_raw ? av_frame_clone(m_raw) : nullptr;
}

Timestamp FrameCommon::pts() const
{
    return {RAW_GET(pts, av::NoPts), m_timeBase};
}

void FrameCommon::setPts(int64_t pts, Rational ptsTimeBase)
{
    RAW_SET(pts, ptsTimeBase.rescale(pts, m_timeBase));
}

void FrameCommon::setPts(const Timestamp &ts)
{
    if (m_timeBase == Rational())
        m_timeBase = ts.timebase();
    RAW_SET(pts, ts.timestamp(m_timeBase));
}

const Rational &FrameCommon::timeBase() const { return m_timeBase; }

void FrameCommon::setTimeBase(const Rational &value) {
    if (m_timeBase == value)
        return;

    if (!m_raw)
        return;

    int64_t rescaledPts          = NoPts;
    int64_t rescaledBestEffortTs = NoPts;

    if (m_timeBase != Rational() && value != Rational()) {
        if (m_raw->pts != av::NoPts)
            rescaledPts = m_timeBase.rescale(m_raw->pts, value);

        if (m_raw->best_effort_timestamp != av::NoPts)
            rescaledBestEffortTs = m_timeBase.rescale(m_raw->best_effort_timestamp, value);
    } else {
        rescaledPts          = m_raw->pts;
        rescaledBestEffortTs = m_raw->best_effort_timestamp;
    }

    if (m_timeBase != Rational()) {
        m_raw->pts                   = rescaledPts;
        m_raw->best_effort_timestamp = rescaledBestEffortTs;
    }

    m_timeBase = value;
}

int FrameCommon::streamIndex() const {
    return m_streamIndex;
}

void FrameCommon::setStreamIndex(int streamIndex) {
    m_streamIndex = streamIndex;
}

void FrameCommon::setComplete(bool isComplete) {
    m_isComplete = isComplete;
}

bool FrameCommon::isComplete() const { return m_isComplete; }

bool FrameCommon::isValid() const {
    return (!isNull() &&
            ((m_raw->data[0] && m_raw->linesize[0]) ||
             ((m_raw->format == AV_PIX_FMT_VAAPI) && ((intptr_t)m_raw->data[3] > 0)))
            );
}

FrameCommon::operator bool() const { return isValid() && isComplete(); }

uint8_t *FrameCommon::data(size_t plane) {
    if (!m_raw || plane >= size_t(AV_NUM_DATA_POINTERS + m_raw->nb_extended_buf))
        return nullptr;
    return m_raw->extended_data[plane];
}

const uint8_t *FrameCommon::data(size_t plane) const {
    if (!m_raw || plane >= size_t(AV_NUM_DATA_POINTERS + m_raw->nb_extended_buf))
        return nullptr;
    return m_raw->extended_data[plane];;
}

size_t FrameCommon::size(size_t plane) const {
    if (!m_raw || plane >= size_t(AV_NUM_DATA_POINTERS + m_raw->nb_extended_buf))
        return 0;
    AVBufferRef *buf = plane < AV_NUM_DATA_POINTERS ?
                           m_raw->buf[plane] :
                           m_raw->extended_buf[plane - AV_NUM_DATA_POINTERS];
    if (buf == nullptr)
        return 0;
    return size_t(buf->size);
}

size_t FrameCommon::size() const {
    if (!m_raw)
        return 0;
    size_t total = 0;
    if (m_raw->buf[0]) {
        for (auto i = 0; i < AV_NUM_DATA_POINTERS && m_raw->buf[i]; i++) {
            total += m_raw->buf[i]->size;
        }

        for (auto i = 0; i < m_raw->nb_extended_buf; ++i) {
            total += m_raw->extended_buf[i]->size;
        }
    } else if (m_raw->data[0]) {
        if (m_raw->width && m_raw->height) {
            uint8_t data[4] = {0};
            int     linesizes[4] = {
                m_raw->linesize[0],
                m_raw->linesize[1],
                m_raw->linesize[2],
                m_raw->linesize[3],
            };
            total = av_image_fill_pointers(reinterpret_cast<uint8_t**>(&data),
                                           static_cast<AVPixelFormat>(m_raw->format),
                                           m_raw->height,
                                           nullptr,
                                           linesizes);
        } else if (m_raw->nb_samples && frame::is_valid_channel_layout(m_raw)) {
            for (auto i = 0; i < m_raw->nb_extended_buf + AV_NUM_DATA_POINTERS && m_raw->extended_data[i]; ++i) {
                // According docs, all planes must have same size
                total += m_raw->linesize[0];
            }
        }
    }
    return total;
}

void FrameCommon::dump() const {
    if (!m_raw)
        return;
    if (m_raw->buf[0]) {
        for (size_t i = 0; i < AV_NUM_DATA_POINTERS && m_raw->buf[i]; i++) {
            av::hex_dump(stdout, m_raw->buf[i]->data, m_raw->buf[i]->size);
        }
    } else if (m_raw->data[0]) {
        av_hex_dump(stdout, m_raw->data[0], size());
    }
}

void FrameCommon::swap(FrameCommon &other)
{
    using std::swap;
#define FRAME_SWAP(x) swap(x, other.x)
    FRAME_SWAP(m_raw);
    FRAME_SWAP(m_timeBase);
    FRAME_SWAP(m_streamIndex);
    FRAME_SWAP(m_isComplete);
#undef FRAME_SWAP
}

#if AVCPP_HAS_FRAME_SIDE_DATA

const FrameSideData FrameCommon::sideData(AVFrameSideDataType type) const
{
    return m_raw ? FrameSideData(av_frame_get_side_data(m_raw, type)) : FrameSideData{};
}

FrameSideData FrameCommon::sideData(AVFrameSideDataType type)
{
    return m_raw ? FrameSideData(av_frame_get_side_data(m_raw, type)) : FrameSideData{};
}

FrameSideData FrameCommon::sideDataIndex(std::size_t index) noexcept
{
    if (!m_raw || index > size_t(m_raw->nb_side_data))
        return {};
    return FrameSideData(m_raw->side_data[index]);
}

void FrameCommon::sideDataRemove(AVFrameSideDataType type) noexcept
{
    if (m_raw)
        av_frame_remove_side_data(m_raw, type);
}

FrameSideData FrameCommon::addSideData(AVFrameSideDataType type, std::span<const uint8_t> data, OptionalErrorCode ec)
{
    // copy and wrap data
    BufferRef ref{data};
    return addSideData(type, std::move(ref), ec);
}

FrameSideData FrameCommon::addSideData(AVFrameSideDataType type, std::span<uint8_t> data, wrap_data, OptionalErrorCode ec)
{
    // just wrap data without copying and take owning
    BufferRef ref{data, av_buffer_default_free, nullptr, 0};
    return addSideData(type, std::move(ref), ec);
}

FrameSideData FrameCommon::addSideData(AVFrameSideDataType type, std::span<uint8_t> data, wrap_data_static, OptionalErrorCode ec)
{
    // just wrap data without copying and take owning
    BufferRef ref{data, av::buffer::null_deleter, nullptr, 0};
    return addSideData(type, std::move(ref), ec);
}

FrameSideData FrameCommon::addSideData(AVFrameSideDataType type, BufferRef buf, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return {};
    }
    auto sd = av_frame_new_side_data_from_buf(m_raw, type, buf.raw());
    if (!sd) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }
    buf.release(); // drop owning
    return FrameSideData{sd};
}

FrameSideData FrameCommon::allocateSideData(AVFrameSideDataType type, std::size_t size, OptionalErrorCode ec)
{
    clear_if(ec);
    if (!m_raw) {
        throws_if(ec, av::Errors::Unallocated);
        return {};
    }

    auto data = av_frame_new_side_data(m_raw, type, size);
    if (!data) {
        throws_if(ec, AVERROR(ENOMEM), ffmpeg_category());
        return {};
    }

    // Data owned by the Frame here, just view
    return FrameSideData(data);
}

ArrayView<const AVFrameSideData *, FrameSideData, size_t> FrameCommon::sideData() const noexcept
{
    if (!m_raw) [[unlikely]]
        return {};
    return make_array_view_size<FrameSideData>((const AVFrameSideData**)m_raw->side_data, m_raw->nb_side_data);
}

ArrayView<AVFrameSideData *, FrameSideData, size_t> FrameCommon::sideData() noexcept
{
    if (!m_raw) [[unlikely]]
        return {};
    return make_array_view_size<FrameSideData>(m_raw->side_data, m_raw->nb_side_data);
}

size_t FrameCommon::sideDataCount() const noexcept
{
    return m_raw ? m_raw->nb_side_data : 0;
}

#endif // AVCPP_HAS_FRAME_SIDE_DATA

void FrameCommon::copyInfoFrom(const FrameCommon &other)
{
    m_timeBase    = other.m_timeBase;
    m_streamIndex = other.m_streamIndex;
    m_isComplete  = other.m_isComplete;
}

void FrameCommon::clone(FrameCommon &dst, size_t align) const
{
    // Setup data required for buffer allocation
    dst.m_raw->format         = m_raw->format;
    dst.m_raw->width          = m_raw->width;
    dst.m_raw->height         = m_raw->height;
    dst.m_raw->nb_samples     = m_raw->nb_samples;

    frame::priv::channel_layout_copy(*dst.m_raw, *m_raw);

    dst.copyInfoFrom(*this);

    av_frame_get_buffer(dst.m_raw, align);
    av_frame_copy(dst.m_raw, m_raw);
    av_frame_copy_props(dst.m_raw, m_raw);
}

#if AVCPP_HAS_FRAME_SIDE_DATA

string_view FrameSideData::name() const noexcept
{
    return m_raw ? name(m_raw->type) : string_view{};
}

AVFrameSideDataType FrameSideData::type() const noexcept
{
    return m_raw ? m_raw->type : AVFrameSideDataType(-1);
}

std::span<const uint8_t> FrameSideData::span() const noexcept
{
    return m_raw ? std::span{m_raw->data, m_raw->size} : std::span<const uint8_t>{};
}

std::span<uint8_t> FrameSideData::span() noexcept
{
    return m_raw ? std::span{m_raw->data, m_raw->size} : std::span<uint8_t>{};
}

BufferRefView FrameSideData::buffer() const noexcept
{
    if (!m_raw) [[unlikely]]
        return {};
    return BufferRefView(m_raw->buf);
}

Dictionary FrameSideData::metadata() const noexcept
{
    return m_raw ? Dictionary{m_raw->metadata, false} : Dictionary{};
}

#if AVCPP_API_HAS_AVSIDEDATADESCRIPTOR
std::optional<AVSideDataDescriptor> FrameSideData::descriptor() const noexcept
{
    return m_raw ? descriptor(m_raw->type) : std::nullopt;
}

std::optional<AVSideDataDescriptor> FrameSideData::descriptor(AVFrameSideDataType type) noexcept
{
    auto desc = av_frame_side_data_desc(type);
    return desc ? std::optional(*desc) : std::nullopt;
}
#endif

bool FrameSideData::empty() const noexcept
{
    return !m_raw || !m_raw->data || !m_raw->size;
}

FrameSideData::operator bool() const noexcept
{
    return !empty();
}

string_view FrameSideData::name(AVFrameSideDataType type) noexcept
{
    auto nm = av_frame_side_data_name(type);
    return nm ? std::string_view{nm} : std::string_view{};
}

#endif // AVCPP_HAS_FRAME_SIDE_DATA

} // ::av
