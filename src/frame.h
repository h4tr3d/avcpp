#ifndef FRAME_H
#define FRAME_H

#include <vector>
#include <memory>
#include <stdexcept>

#include "ffmpeg.h"
#include "rational.h"

extern "C" {
#include "libavutil/imgutils.h"
}

namespace av
{

template<typename T>
class Frame2 : public FFWrapperPtr<AVFrame>
{
protected:
    T& assignOperator(const T &rhs) {
        if (this == &rhs)
            return static_cast<T&>(*this);
        T(rhs).swap(static_cast<T&>(*this));
        return static_cast<T&>(*this);
    }

    T& moveOperator(T &&rhs) {
        if (this == &rhs)
            return static_cast<T&>(*this);
        T(std::move(rhs)).swap(static_cast<T&>(*this));
        return static_cast<T&>(*this);
    }

public:
    Frame2() {
        m_raw = av_frame_alloc();
        m_raw->opaque = this;
    }

    ~Frame2() {
        av_frame_free(&m_raw);
    }

    Frame2(const AVFrame *frame) {
        if (frame) {
            m_raw = av_frame_alloc();
            m_raw->opaque = this;
            av_frame_ref(m_raw, frame);
        }
    }

    // Helper ctors to implement move/copy ctors
    Frame2(const T& other) : Frame2(other.m_raw) {
        if (m_raw)
            copyInfoFrom(other);
    }

    Frame2(T&& other) : FFWrapperPtr<AVFrame>(nullptr) {
        if (other.m_raw) {
            m_raw = av_frame_alloc();
            m_raw->opaque = this;
            av_frame_move_ref(m_raw, other.m_raw);
            copyInfoFrom(other);
        }
    }

    static T null() {
        return T(nullptr);
    }

    // You must implement operators in deveritive classes using assignOperator() and moveOperator()
    void operator=(const Frame2&) = delete;

    void swap(Frame2 &other) {
        using std::swap;
#define FRAME_SWAP(x) swap(x, other.x)
        FRAME_SWAP(m_raw);
        FRAME_SWAP(m_timeBase);
        FRAME_SWAP(m_streamIndex);
        FRAME_SWAP(m_isComplete);
        FRAME_SWAP(m_fakePts);
#undef FRAME_SWAP
    }

    void copyInfoFrom(const T& other) {
        m_timeBase    = other.m_timeBase;
        m_streamIndex = other.m_streamIndex;
        m_isComplete  = other.m_isComplete;
        m_fakePts     = other.m_fakePts;
    }

    bool isReferenced() const {
        return m_raw && m_raw->buf[0];
    }

    int refCount() const {
        if (m_raw && m_raw->buf[0])
            return av_buffer_get_ref_count(m_raw->buf[0]);
        else
            return 0;
    }

    AVFrame* makeRef() const {
        return m_raw ? av_frame_clone(m_raw) : nullptr;
    }

    T clone(size_t align = 1) const {
        T result;

        // Setup data required for buffer allocation
        result.m_raw->format         = m_raw->format;
        result.m_raw->width          = m_raw->width;
        result.m_raw->height         = m_raw->height;
        result.m_raw->nb_samples     = m_raw->nb_samples;
        result.m_raw->channel_layout = m_raw->channel_layout;
        result.m_raw->channels       = m_raw->channels;

        result.copyInfoFrom(static_cast<const T&>(*this));

        av_frame_get_buffer(result.m_raw, align);
        av_frame_copy(result.m_raw, m_raw);
        av_frame_copy_props(result.m_raw, m_raw);
        return result;
    }

    int64_t pts() const {
        return RAW_GET(pts, AV_NOPTS_VALUE);
    }

    void setPts(int64_t pts) {
        RAW_SET(pts, pts);
    }

    void setPts(int64_t pts, Rational ptsTimeBase) {
        RAW_SET(pts, ptsTimeBase.rescale(pts, m_timeBase));
    }

    const Rational& timeBase() const { return m_timeBase; }

    void setTimeBase(const Rational &value) {
        if (m_timeBase == value)
            return;

        if (!m_raw)
            return;

        int64_t rescaledPts          = AV_NOPTS_VALUE;
        int64_t rescaledFakePts      = AV_NOPTS_VALUE;
        int64_t rescaledBestEffortTs = AV_NOPTS_VALUE;

        if (m_timeBase != Rational() && value != Rational()) {
            if (m_raw->pts != AV_NOPTS_VALUE)
                rescaledPts = m_timeBase.rescale(m_raw->pts, value);

            if (m_raw->best_effort_timestamp != AV_NOPTS_VALUE)
                rescaledBestEffortTs = m_timeBase.rescale(m_raw->best_effort_timestamp, value);

            if (m_fakePts != AV_NOPTS_VALUE)
                rescaledFakePts = m_timeBase.rescale(m_fakePts, value);
        } else {
            rescaledPts          = m_raw->pts;
            rescaledFakePts      = m_fakePts;
            rescaledBestEffortTs = m_raw->best_effort_timestamp;
        }

        if (m_timeBase != Rational()) {
            m_raw->pts                   = rescaledPts;
            m_raw->best_effort_timestamp = rescaledBestEffortTs;
            m_fakePts                    = rescaledFakePts;
        }

        m_timeBase = value;
    }

    int streamIndex() const {
        return m_streamIndex;
    }

    void setStreamIndex(int streamIndex) {
        m_streamIndex = streamIndex;
    }

    void setComplete(bool isComplete) {
        m_isComplete = isComplete;
    }

    bool isComplete() const { return m_isComplete; }

    bool isValid() const { return (!isNull() && m_raw->data[0] && m_raw->linesize[0]); }

    operator bool() const { return isValid() && isComplete(); }

    uint8_t *data(size_t plane = 0) {
        if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
            return nullptr;
        return m_raw->data[plane];
    }

    const uint8_t *data(size_t plane = 0) const {
        if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
            return nullptr;
        return m_raw->data[plane];
    }

    size_t size(size_t plane) const {
        if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
            return 0;
        AVBufferRef *buf = m_raw->buf[plane];
        if (buf == nullptr)
            return 0;
        return buf->size;
    }

    size_t size() const {
        if (!m_raw)
            return 0;
        size_t total = 0;
        if (m_raw->buf[0]) {
            for (size_t i = 0; i < AV_NUM_DATA_POINTERS && m_raw->buf[i]; i++) {
                total += m_raw->buf[i]->size;
            }
        } else if (m_raw->data[0]) {
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
        }
        return total;
    }

    void dump() const {
        if (!m_raw)
            return;
        if (m_raw->buf[0]) {
            for (size_t i = 0; i < AV_NUM_DATA_POINTERS && m_raw->buf[i]; i++) {
                av_hex_dump(stdout, m_raw->buf[i]->data, m_raw->buf[i]->size);
            }
        } else if (m_raw->data[0]) {
            av_hex_dump(stdout, m_raw->data[0], size());
        }
    }

protected:
    Rational             m_timeBase;
    int                  m_streamIndex {-1};
    bool                 m_isComplete  {false};
    int64_t              m_fakePts     {AV_NOPTS_VALUE};
};


class VideoFrame2 : public Frame2<VideoFrame2>
{
public:
    using Frame2<VideoFrame2>::Frame2;

    VideoFrame2() = default;
    VideoFrame2(AVPixelFormat pixelFormat, int width, int height, int align = 1);
    VideoFrame2(const uint8_t *data, size_t size, AVPixelFormat pixelFormat, int width, int height, int align = 1);

    VideoFrame2(const VideoFrame2 &other);
    VideoFrame2(VideoFrame2 &&other);

    VideoFrame2& operator=(const VideoFrame2 &rhs);
    VideoFrame2& operator=(VideoFrame2 &&rhs);

    AVPixelFormat          pixelFormat() const;
    int                    width() const;
    int                    height() const;

    bool                   isKeyFrame() const;
    void                   setKeyFrame(bool isKey);

    int                    quality() const;
    void                   setQuality(int quality);

    AVPictureType          pictureType() const;
    void                   setPictureType(AVPictureType type = AV_PICTURE_TYPE_NONE);
};


class AudioSamples2 : public Frame2<AudioSamples2>
{
public:
    using Frame2<AudioSamples2>::Frame2;

    AudioSamples2() = default;
    AudioSamples2(AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = 1);
    AudioSamples2(const uint8_t *data, size_t size,
                  AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = 1);

    AudioSamples2(const AudioSamples2 &other);
    AudioSamples2(AudioSamples2 &&other);

    AudioSamples2& operator=(const AudioSamples2 &rhs);
    AudioSamples2& operator=(AudioSamples2 &&rhs);

    int            init(AVSampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = 1);

    AVSampleFormat sampleFormat() const;
    int            samplesCount() const;
    int            channelsCount() const;
    int64_t        channelsLayout() const;
    int            sampleRate() const;
    uint           sampleBitDepth() const;

    std::string    channelsLayoutString() const;

    void           setFakePts(int64_t pts);
    int64_t        fakePts() const;
};


} // ::av

#endif // FRAME_H
