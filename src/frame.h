#ifndef FRAME_H
#define FRAME_H

#include <vector>
#include <memory>
#include <stdexcept>

#include "ffmpeg.h"
#include "rational.h"

namespace av
{

template<typename T>
class Frame2 : public FFWrapperPtr<AVFrame>
{
protected:
    void setupFrom(const T& other) {
        m_timeBase    = other.m_timeBase;
        m_streamIndex = other.m_streamIndex;
        m_isComplete  = other.m_isComplete;
        m_fakePts     = other.m_fakePts;
    }

public:
    Frame2() {
        m_raw = av_frame_alloc();
        m_raw->opaque = this;
    }

    ~Frame2() {
        av_frame_free(&m_raw);
    }

    Frame2(const AVFrame *frame) : Frame2() {
        av_frame_ref(m_raw, frame);
    }

    Frame2(const T& other) : Frame2(other.m_raw) {
        setupFrom(other);
    }

    Frame2(T&& other) : Frame2() {
        av_frame_move_ref(m_raw, other.m_raw);
        setupFrom(other);
    }

    T& operator=(const T &rhs) {
        if (this == &rhs)
            return *this;
        T(rhs).swap(*this);
        return static_cast<T&>(*this);
    }

    T& operator=(T &&rhs) {
        if (this == &rhs)
            return *this;
        T(std::move(rhs)).swap(*this);
        return static_cast<T&>(*this);
    }

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

    bool isReferenced() const {
        return m_raw->buf[0];
    }

    int refCount() const {
        if (m_raw->buf[0])
            return av_buffer_get_ref_count(m_raw->buf[0]);
        else
            return 0;
    }

    AVFrame* makeRef() const {
        return av_frame_clone(m_raw);
    }

    T clone() const {
        T result;

        // Setup data required for buffer allocation
        result.m_raw->format         = m_raw->format;
        result.m_raw->width          = m_raw->width;
        result.m_raw->height         = m_raw->height;
        result.m_raw->nb_samples     = m_raw->nb_samples;
        result.m_raw->channel_layout = m_raw->channel_layout;
        result.m_raw->channels       = m_raw->channels;

        av_frame_get_buffer(result.m_raw, 32);
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

    const Rational& timeBase() const { return m_timeBase; }

    void setTimeBase(const Rational &value) {
        if (m_timeBase == value)
            return;

        if (!m_raw)
            return;

        int64_t rescaledPts          = AV_NOPTS_VALUE;
        int64_t rescaledFakePts      = AV_NOPTS_VALUE;
        int64_t rescaledBestEffortTs = AV_NOPTS_VALUE;

        if (m_raw) {
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
        }

        m_timeBase = value;


        m_raw->pts                   = rescaledPts;
        m_raw->best_effort_timestamp = rescaledBestEffortTs;
        m_fakePts                    = rescaledFakePts;
    }

    int streamIndex() const {
        return m_streamIndex;
    }

    void setStreamIndex(int streamIndex) {
        m_streamIndex = streamIndex;
    }

    void setComplete(bool isComplete) {
        m_isComplete = true;
    }

    bool isComplete() const { return m_isComplete; }

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
            // TODO: calc size
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

    VideoFrame2(AVPixelFormat pixelFormat, int width, int height, int align = 1);
    VideoFrame2(const uint8_t *data, size_t size, AVPixelFormat pixelFormat, int width, int height, int align = 1) throw(std::length_error);

    AVPixelFormat          pixelFormat() const;
    int                    width() const;
    int                    height() const;

    bool                   isKeyFrame() const;
    void                   setKeyFrame(bool isKey);

    int                    quality() const;
    void                   setQuality(int quality);

    AVPictureType          pictureType() const;
    void                   setPictureType(AVPictureType type);
};


class AudioSamples2 : public Frame2<AudioSamples2>
{
public:
    using Frame2<AudioSamples2>::Frame2;

    AudioSamples2(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate, int align = 1);
    AudioSamples2(const uint8_t *data, size_t size,
                  AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate, int align = 1) throw(std::length_error);

    AVSampleFormat sampleFormat() const;
    int            samplesCount() const;
    int            channelsCount() const;
    int64_t        channelsLayout() const;
    int            sampleRate() const;
    uint           sampleBitDepth() const;
};


/**
 * Base class for VideoFrame and AudioSample. In low both of them is AVFrame, but with some
 * differences, like getSize(), validation logic, setting up data pointers.
 *
 */
class Frame
{
public:
    /**
     * @brief Frame
     *
     * Default ctor
     *
     */
    Frame();
    virtual ~Frame();

    // virtual
    /**
     * Calculate size for frame data, it different for video (avpicture_get_size) and audio
     * (av_samples_get_buffer_size) frames so it need to implemented in extended classes.
     *
     * @return frame data size or -1 if it can not be calculated.
     */
    virtual int                      getSize() const = 0;
    /**
     * Check frame for valid
     * @return true if frame valid false otherwise
     */
    virtual bool                     isValid() const = 0;
    /**
     * Make frame duplicate
     * @return new frame pointer or null if frame can not be created
     */
    virtual std::shared_ptr<Frame> clone()         = 0;

    // common
    virtual int64_t  getPts() const;
    virtual void     setPts(int64_t pts);

    int64_t getBestEffortTimestamp() const;

    // Some frames must have PTS value setted to AV_NOPTS_VALUE (like audio frames).
    // But, in some situations we should have access to realc or calculated PTS values.
    // So we use FakePts as a name for this PTS values. By default, setPts() methods also set
    // fakePts value but setFakePts() method only set fakePts value.
    virtual int64_t getFakePts() const;
    virtual void    setFakePts(int64_t pts);

    // Non-virtual
    Rational&       getTimeBase() { return m_timeBase; }
    const Rational& getTimeBase() const { return m_timeBase; }
    void            setTimeBase(const Rational &value);

    int             getStreamIndex() const;
    void            setStreamIndex(int m_streamIndex);

    void            setComplete(bool isComplete);
    bool            isComplete() const { return m_completeFlag; }

    void dump();

    const AVFrame*              getAVFrame() const;
    AVFrame*                    getAVFrame();
    const std::vector<uint8_t>& getFrameBuffer() const;


    // Operators
    Frame& operator= (const AVFrame *m_frame);
    Frame& operator= (const Frame &m_frame);

protected:
    // virtual
    virtual void setupDataPointers(const AVFrame *frame) = 0;

    // common
    void initFromAVFrame(const AVFrame *frame);
    void allocFrame();

protected:
    AVFrame*             m_frame;
    std::vector<uint8_t> m_frameBuffer;

    Rational             m_timeBase;
    int                  m_streamIndex;

    bool                 m_completeFlag;

    int64_t              m_fakePts;
};

typedef std::shared_ptr<Frame> FramePtr;
typedef std::weak_ptr<Frame> FrameWPtr;

} // ::av

#endif // FRAME_H
