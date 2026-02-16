#pragma once

#include <vector>

#include "avcompat.h"

#if AVCPP_CXX_STANDARD >= 20
#if __has_include(<span>)
#include <span>
#endif
#endif

#include "ffmpeg.h"
#include "rational.h"
#include "timestamp.h"
#include "pixelformat.h"
#include "sampleformat.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/attributes.h>
}

namespace av
{

namespace frame
{
int64_t get_best_effort_timestamp(const AVFrame* frame);
} // ::av::frame

class FrameCommon : public FFWrapperPtr<AVFrame>
{
public:
    FrameCommon();
    FrameCommon(const AVFrame *frame);

    // Helper ctors to implement move/copy ctors
    FrameCommon(const FrameCommon& other);
    FrameCommon(FrameCommon&& other);

    ~FrameCommon();

    bool isReferenced() const;
    int refCount() const;
    AVFrame* makeRef() const;

    Timestamp pts() const;
    attribute_deprecated void setPts(int64_t pts, Rational ptsTimeBase);
    void setPts(const Timestamp &ts);

    const Rational& timeBase() const;
    void setTimeBase(const Rational &value);

    int streamIndex() const;
    void setStreamIndex(int streamIndex);

    void setComplete(bool isComplete);
    bool isComplete() const;

    bool isValid() const;

    operator bool() const;

    uint8_t *data(size_t plane = 0);
    const uint8_t *data(size_t plane = 0) const;

    size_t size(size_t plane) const;
    size_t size() const;

    void dump() const;

    // You must implement operators in deveritive classes using assignOperator() and moveOperator()
    void operator=(const FrameCommon&) = delete;

    void swap(FrameCommon &other);

protected:
    void copyInfoFrom(const FrameCommon& other);
    void clone(FrameCommon& dst, size_t align = 1) const;

protected:
    Rational             m_timeBase{};
    int                  m_streamIndex {-1};
    bool                 m_isComplete  {false};
};

template<typename T>
class Frame : public FrameCommon
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
    using FrameCommon::FrameCommon;

    static T null() {
        return T(nullptr);
    }

    T clone(size_t align = 1) const {
        T result;
        FrameCommon::clone(result, align);
        return result;
    }
};

static_assert(std::is_copy_assignable_v<FrameCommon> == false);
static_assert(std::is_copy_constructible_v<FrameCommon> == true);
static_assert(std::is_move_assignable_v<FrameCommon> == false);
static_assert(std::is_move_constructible_v<FrameCommon> == true);

class VideoFrame : public Frame<VideoFrame>
{
public:
    using Frame<VideoFrame>::Frame;

    VideoFrame() = default;
    VideoFrame(PixelFormat pixelFormat, int width, int height, int align = 1);
    VideoFrame(const uint8_t *data, size_t size, PixelFormat pixelFormat, int width, int height, int align = 1);

    VideoFrame(const VideoFrame &other);
    VideoFrame(VideoFrame &&other);

    VideoFrame& operator=(const VideoFrame &rhs);
    VideoFrame& operator=(VideoFrame &&rhs);

    PixelFormat            pixelFormat() const;
    int                    width() const;
    int                    height() const;

    /**
     * Convert JPEG pixel format to the compatible non-JPEG one
     *
     * YUV Pixel formats with J in the naming (AV_PIX_FMT_YUVJxxx) mostly deprecated and some components warn about it.
     * Allow to adjust internal pixel format into compatible one.
     *
     * @return previous pixel format
     */
    PixelFormat            adjustFromJpegPixelFormat();
    /**
     * Convert non-JPEG pixel format to the compatible JPEG one
     *
     * YUV Pixel formats with J in the naming (AV_PIX_FMT_YUVJxxx) mostly deprecated and some components warn about it.
     * Allow to adjust internal pixel format into compatible one.
     *
     * @return previous pixel format
     */
    PixelFormat            adjustToJpegPixelFormat();

    bool                   isKeyFrame() const;
    void                   setKeyFrame(bool isKey);

    int                    quality() const;
    void                   setQuality(int quality);

    AVPictureType          pictureType() const;
    void                   setPictureType(AVPictureType type = AV_PICTURE_TYPE_NONE);

    Rational               sampleAspectRatio() const;
    void                   setSampleAspectRatio(const Rational& sampleAspectRatio);

    size_t                 bufferSize(int align = 1, OptionalErrorCode ec = throws()) const;
    bool                   copyToBuffer(uint8_t *dst, size_t size, int align = 1, OptionalErrorCode ec = throws());
    bool                   copyToBuffer(std::vector<uint8_t>& dst, int align = 1, OptionalErrorCode ec = throws());


    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired.
     *
     * @param data          data pointer to wrap
     * @param size          whole data buffer size with alignment and real data
     * @param pixelFormat   pixel format of the data
     * @param width         image width
     * @param height        image height
     * @param align         image data alignemnt
     *
     * @return image data wrapped into VideoFrame object
     */
    static VideoFrame wrap(const void *data, size_t size, PixelFormat pixelFormat, int width, int height, int align = 1);

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired.

     * @param data           external image data wrapped into std::span
     * @param pixelFormat    pixel format of the data
     * @param width          image width
     * @param height         image height
     * @param align          image data alignment
     * @return
     */
    static VideoFrame wrap(std::span<const std::byte> data, PixelFormat pixelFormat, int width, int height, int align = 1);

    /**
     * Wrap external data into VideoFrame object ready to use with FFmpeg/AvCpp.
     *
     * @note Ownershipping does not acquired. When data wants to be deleted, passed deleter function will be called.

     * @param data           external image data wrapped into std::span
     * @param deleter        user defined deleter
     * @param opaque         opaque data passed to the deleter, maybe nullptr
     * @param pixelFormat    pixel format of the data
     * @param width          image width
     * @param height         image height
     * @param align          image data alignment
     * @return
     */
    static VideoFrame wrap(std::span<const std::byte> data, void (*deleter)(void *opaque, uint8_t *data),
                           void *opaque, PixelFormat pixelFormat, int width, int height, int align = 1);
#endif
};

static_assert(std::is_copy_assignable_v<VideoFrame> == true);
static_assert(std::is_copy_constructible_v<VideoFrame> == true);
static_assert(std::is_move_assignable_v<VideoFrame> == true);
static_assert(std::is_move_constructible_v<VideoFrame> == true);

class AudioSamples : public Frame<AudioSamples>
{
public:
    using Frame<AudioSamples>::Frame;

    AudioSamples() = default;
    AudioSamples(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);
    AudioSamples(const uint8_t *data, size_t size,
                  SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);

    AudioSamples(const AudioSamples &other);
    AudioSamples(AudioSamples &&other);

    AudioSamples& operator=(const AudioSamples &rhs);
    AudioSamples& operator=(AudioSamples &&rhs);

    int            init(SampleFormat sampleFormat, int samplesCount, uint64_t channelLayout, int sampleRate, int align = SampleFormat::AlignDefault);

    SampleFormat   sampleFormat() const;
    int            samplesCount() const;
    int            channelsCount() const;
    uint64_t       channelsLayout() const;
    int            sampleRate() const;
    size_t         sampleBitDepth(OptionalErrorCode ec = throws()) const;
    bool           isPlanar() const;

    std::string    channelsLayoutString() const;
};

static_assert(std::is_copy_assignable_v<AudioSamples> == true);
static_assert(std::is_copy_constructible_v<AudioSamples> == true);
static_assert(std::is_move_assignable_v<AudioSamples> == true);
static_assert(std::is_move_constructible_v<AudioSamples> == true);


} // ::av

