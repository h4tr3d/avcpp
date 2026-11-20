#pragma once

/**
 * Codec parameters shared between streams and contexts.
 *
 * @file
 * @ingroup codec
 */

#include "averror.h"
#include "ffmpeg.h"
#include "avutils.h"
#include "codec.h"

extern "C" {
// codec_par.h included if needed
#include <libavcodec/avcodec.h>
}

namespace av {

/**
 * Direction a codec or a stream is used in.
 *
 * The value is part of the codec context type, so passing a decoder context
 * where an encoder is expected does not compile.
 */
enum class Direction
{
    /// Not a direction; the default state of a context that has none.
    Invalid = -1,
    /// Compressing frames into packets.
    Encoding,
    /// Decompressing packets into frames.
    Decoding
};

/**
 * @brief The CodecParametersView class
 *
 * Not-owned wrapper for AVCodecParameters. Do not keep for a long time.
 */
class CodecParametersView : public FFWrapperPtr<AVCodecParameters>
{
public:
    /**
     * Wrap FFmpeg codec parameters.
     *
     * @param codecpar the parameters to view; null produces an invalid view
     *
     * @note Non-owning: the parameters must outlive the view.
     */
    CodecParametersView(AVCodecParameters *codecpar = nullptr);
    /**
     * Whether the view refers to parameters.
     *
     * @return `true` when the underlying AVCodecParameters exists
     */
    bool isValid() const;

    /**
     * Copy parameters into this view from another view.
     *
     * @param src the parameters to copy
     * @param ec  holder for the error code
     */
    void copyFrom(CodecParametersView src, OptionalErrorCode ec = throws());
    /**
     * Copy these parameters into another view.
     *
     * @param dst the parameters to fill
     * @param ec  holder for the error code
     */
    void copyTo(CodecParametersView dst, OptionalErrorCode ec = throws()) const;

    /**
     * Copy parameters from a codec context.
     *
     * @param src the context to copy from
     * @param ec  holder for the error code
     */
    void copyFrom(const class CodecContext2& src, OptionalErrorCode ec = throws());
    /**
     * Copy these parameters into a codec context.
     *
     * @param dst the context to fill
     * @param ec  holder for the error code
     */
    void copyTo(class CodecContext2& dst, OptionalErrorCode ec = throws()) const;

    /**
     * Duration of one audio frame of the given size.
     *
     * @param frame_bytes size of the frame in bytes
     *
     * @return the duration in the stream time base, or a negative value when the
     *         parameters do not describe audio
     */
    int getAudioFrameDuration(int frame_bytes) const;

    // Getters & Setters

    /**
     * Media type the codec handles.
     *
     * @return the `AVMEDIA_TYPE_*` value
     */
    AVMediaType  codecType() const;
    /**
     * Set the media type.
     *
     * @param codec_type the `AVMEDIA_TYPE_*` value
     */
    void         codecType(AVMediaType codec_type);

    /**
     * Media type of the parameters.
     *
     * Kept for symmetry with codecType(); FFmpeg stores one value for both.
     *
     * @return the `AVMEDIA_TYPE_*` value
     */
    AVMediaType  mediaType() const;
    /**
     * Set the media type.
     *
     * @param media_type the `AVMEDIA_TYPE_*` value
     */
    void         mediaType(AVMediaType media_type);

    /**
     * Identifier of the codec.
     *
     * @return the `AV_CODEC_ID_*` value
     */
    AVCodecID    codecId() const;
    /**
     * Set the codec identifier.
     *
     * @param codec_id the `AV_CODEC_ID_*` value
     */
    void         codecId(AVCodecID codec_id);

    /**
     * Encoder for this codec identifier.
     *
     * @return the codec descriptor, or an invalid one when no encoder exists
     */
    Codec        encodingCodec() const;
    /**
     * Decoder for this codec identifier.
     *
     * @return the codec descriptor, or an invalid one when no decoder exists
     */
    Codec        decodingCodec() const;

    /**
     * Four-character code of the codec inside the container.
     *
     * @return the tag, or zero when it is not set
     */
    uint32_t     codecTag() const;
    /**
     * Set the container codec tag.
     *
     * @param codec_tag the tag to store; zero clears it, which lets the muxer
     *                  choose a tag for its own container
     */
    void         codecTag(uint32_t codec_tag);

    // TBD
};

/**
 * Owning codec parameters.
 *
 * Allocates the FFmpeg structure on construction and frees it on destruction.
 * Use it when parameters have to outlive the stream or the context they came
 * from.
 */
class CodecParameters : public CodecParametersView, public noncopyable
{
public:
    /**
     * Allocate empty parameters.
     */
    CodecParameters();
    /**
     * Free the owned parameters.
     */
    ~CodecParameters();
};

} // namespace av
