#pragma once

/**
 * Streams inside a container: parameters, time base and disposition.
 *
 * @file
 * @ingroup avformat
 */

#include <memory>

#include "averror.h"
#include "ffmpeg.h"
#include "rational.h"
#include "timestamp.h"
#include "codecparameters.h"

#if AVCPP_HAS_AVFORMAT

namespace av
{
/**
 * One stream inside a container.
 *
 * A stream is a view: it points into the AVFormatContext that owns the
 * AVStream, so it must not outlive that context. Copying a Stream is cheap and
 * does not copy any stream data.
 *
 * Every value here has meaning only after FormatContext::findStreamInfo(): a
 * container read only far enough to list its streams reports incomplete codec
 * parameters and zeroed timings.
 */
class Stream : public FFWrapperPtr<AVStream>
{
private:
    friend class FormatContext;
    Stream(const std::shared_ptr<char> &monitor, AVStream *st = nullptr, Direction direction = Direction::Invalid);

public:
    Stream() = default;

    /**
     * Whether the view refers to a stream.
     *
     * @return `true` when the underlying AVStream exists
     */
    bool isValid() const;

    /**
     * Position of the stream in its container.
     *
     * @return the index used to match packets and frames to this stream
     */
    int index() const;
    /**
     * Stream identifier from the container.
     *
     * Unlike index(), the id is stable across containers and is what muxers
     * store, but FFmpeg does not require it to be unique.
     *
     * @return the container's stream id
     */
    int id() const;

    /**
     * Frame rate declared by the stream.
     *
     * @return frames per second, or an empty value when the stream does not
     *         declare one
     */
    Rational    frameRate()          const;
    /**
     * Time base of the timestamps in this stream.
     *
     * @return timestamp units per second
     */
    Rational    timeBase()           const;
    /**
     * Pixel aspect ratio declared by the stream.
     *
     * @return width divided by height of one pixel, or an empty value when it is
     *         not declared
     */
    Rational    sampleAspectRatio()  const;
    /**
     * Average frame rate of the stream.
     *
     * Encoders and muxers prefer the average over the nominal rate for variable
     * frame rate content.
     *
     * @return frames per second, or an empty value when it is unknown
     */
    Rational    averageFrameRate()   const;
    /**
     * Timestamp of the first frame in this stream.
     *
     * @return the start time in the stream time base, or NoPts when unknown
     */
    Timestamp   startTime()          const;
    /**
     * Duration of the stream.
     *
     * @return the duration in the stream time base, or NoPts when unknown
     */
    Timestamp   duration()           const;

    /**
     * Decoding timestamp of the packet currently being processed by this stream.
     *
     * @deprecated For FFmpeg 5.0 and newer this returns NO_PTS: take the DTS from the packet
     *             instead.
     *
     * @return the decoding timestamp, or NO_PTS when the stream does not provide one
     */
    [[deprecated("For FFmpeg 5.0+ return NO_PTS")]]
    Timestamp   currentDts()         const;
    /**
     * Kind of media the stream carries.
     *
     * @return the `AVMEDIA_TYPE_*` value
     */
    AVMediaType mediaType()          const;

    /**
     * Whether this is an audio stream.
     *
     * @return `true` for `AVMEDIA_TYPE_AUDIO`
     */
    bool isAudio()      const;
    /**
     * Whether this is a video stream.
     *
     * @return `true` for `AVMEDIA_TYPE_VIDEO`
     */
    bool isVideo()      const;
    /**
     * Whether this is a data stream.
     *
     * @return `true` for `AVMEDIA_TYPE_DATA`
     */
    bool isData()       const;
    /**
     * Whether this is a subtitle stream.
     *
     * @return `true` for `AVMEDIA_TYPE_SUBTITLE`
     */
    bool isSubtitle()   const;
    /**
     * Whether this is an attachment stream, such as an embedded font.
     *
     * @return `true` for `AVMEDIA_TYPE_ATTACHMENT`
     */
    bool isAttachment() const;

    /**
     * Direction the stream is used in.
     *
     * @return Encoding for an output stream, Decoding for an input one
     */
    Direction   direction() const { return m_direction; }

    /**
     * Set the time base of the stream.
     *
     * @param timeBase timestamp units per second; must be set before the header
     *                 of an output container is written
     */
    void setTimeBase(const Rational &timeBase);
    /**
     * Set the declared frame rate.
     *
     * @param frameRate frames per second
     */
    void setFrameRate(const Rational &frameRate);
    /**
     * Set the pixel aspect ratio.
     *
     * @param aspectRatio width divided by height of one pixel
     */
    void setSampleAspectRatio(const Rational &aspectRatio);
    /**
     * Set the average frame rate.
     *
     * @param frameRate frames per second
     */
    void setAverageFrameRate(const Rational &frameRate);

    /**
     * Codec parameters of the stream.
     *
     * @return a non-owning view; it stays valid while the container is open
     */
    CodecParametersView codecParameters() const;
    /**
     * Copy codec parameters into the stream.
     *
     * This is how a muxer learns what the packets will contain during
     * remuxing, without opening any codec.
     *
     * @param codecpar the parameters to copy
     * @param ec       holder for the error code
     */
    void setCodecParameters(CodecParametersView codecpar, OptionalErrorCode ec = throws());

    /**
     * Flags to the user to detect events happening on the stream.
     * A combination of AVSTREAM_EVENT_FLAG_*. Must be cleared by the user.
     * @see AVFormatContext::event_flags
     * @return
     */
    /**
     * Events FFmpeg raised for this stream, as a bit mask.
     *
     * @return a combination of `AVSTREAM_EVENT_FLAG_*`; the caller must clear it
     */
    int eventFlags() const noexcept;
    /**
     * Whether any of the given events happened.
     *
     * @param flags the `AVSTREAM_EVENT_FLAG_*` bits to test
     *
     * @return `true` when at least one of the flags is set
     */
    bool eventFlags(int flags) const noexcept;
    /**
     * Acknowledge events.
     *
     * @param flags the bits to clear
     */
    void eventFlagsClear(int flags) noexcept;

    /**
     * Configure an output stream from a video encoder.
     *
     * Copies the encoder parameters into the stream: size, pixel format, frame
     * rate, aspect ratio and time base.
     *
     * @param ctx an opened encoder
     * @param ec  holder for the error code
     */
    void setupEncodingParameters(const class VideoEncoderContext& ctx, OptionalErrorCode ec = throws());

private:
    /// Weak reference to the owning container, used to detect a dangling view.
    std::weak_ptr<char> m_parentMonitor;
    Direction           m_direction = Direction::Invalid;
};

} // ::av

#endif // if AVCPP_HAS_AVFORMAT
