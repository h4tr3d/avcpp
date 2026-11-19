#pragma once

/**
 * Codec descriptors and lookup by name or id.
 *
 * @file
 * @ingroup codec
 */

#include <list>
#include <deque>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "format.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "channellayout.h"
#include "avutils.h"

namespace av {

/**
 * Codec descriptor.
 *
 * Points at the static description FFmpeg registers for a codec: its name, what
 * it can do, and which formats it accepts. The library owns the descriptor, the
 * wrapper only points at it, so copying a Codec copies a pointer.
 */
class Codec : public FFWrapperPtr<const AVCodec>
{
public:
    using FFWrapperPtr<const AVCodec>::FFWrapperPtr;

    /**
     * Short name of the codec, as FFmpeg knows it.
     *
     * @return a static string such as "h264", or nullptr for an invalid codec
     */
    const char *name() const;
    /**
     * Human-readable name of the codec.
     *
     * @return a static description, or nullptr for an invalid codec
     */
    const char *longName() const;
    /**
     * Whether the codec can be used for encoding.
     *
     * @return `true` when FFmpeg registers an encoder with this name
     */
    bool        canEncode() const;
    /**
     * Whether the codec can be used for decoding.
     *
     * @return `true` when FFmpeg registers a decoder with this name
     */
    bool        canDecode() const;
    /**
     * Whether this descriptor is an encoder.
     *
     * @return `true` for encoder registrations, which is how `h264_qsv` and
     *         `h264` are told apart
     */
    bool        isEncoder() const;
    /**
     * Whether this descriptor is a decoder.
     *
     * @return `true` for decoder registrations
     */
    bool        isDecoder() const;
    /**
     * Media type the codec handles.
     *
     * @return the `AVMEDIA_TYPE_*` value
     */
    AVMediaType type() const;

    /**
     * Frame rates the encoder supports.
     *
     * @return the supported rates; empty when the codec does not restrict them
     */
    std::deque<Rational>       supportedFramerates()    const;
    /**
     * Pixel formats the codec supports.
     *
     * @return the formats; empty when they are not known before opening
     */
    std::deque<PixelFormat>    supportedPixelFormats()  const;
    /**
     * Sample rates the codec supports.
     *
     * @return the rates; empty when the codec accepts any rate
     */
    std::deque<int>            supportedSamplerates()   const;
    /**
     * Sample formats the codec supports.
     *
     * @return the formats; empty when they are not known before opening
     */
    std::deque<SampleFormat>   supportedSampleFormats() const;
    /**
     * Channel layouts the codec supports, as masks.
     *
     * @return the layouts; empty when the codec accepts any layout
     */
    std::deque<uint64_t>       supportedChannelLayouts() const;

#if AVCPP_API_NEW_CHANNEL_LAYOUT
    /**
     * Channel layouts the codec supports, as structured views.
     *
     * Available with the modern FFmpeg channel layout API.
     *
     * @return non-owning views of the layouts; empty when any layout is accepted
     */
    std::deque<ChannelLayoutView> supportedChannelLayouts2() const;
#endif

    /**
     * Identifier of the codec.
     *
     * @return the `AV_CODEC_ID_*` value
     */
    AVCodecID id() const;
};



/**
 * Find an encoder by codec identifier.
 *
 * @param id the `AV_CODEC_ID_*` value
 *
 * @return the encoder, or an invalid codec when none exists
 */
Codec findEncodingCodec(AVCodecID id);
/**
 * Find an encoder by name.
 *
 * @param name the encoder name, for example "libx264"
 *
 * @return the encoder, or an invalid codec when the name is unknown
 */
Codec findEncodingCodec(const std::string& name);

/**
 * Find a decoder by codec identifier.
 *
 * @param id the `AV_CODEC_ID_*` value
 *
 * @return the decoder, or an invalid codec when none exists
 */
Codec findDecodingCodec(AVCodecID id);
/**
 * Find a decoder by name.
 *
 * @param name the decoder name, for example "h264"
 *
 * @return the decoder, or an invalid codec when the name is unknown
 */
Codec findDecodingCodec(const std::string& name);

#if AVCPP_HAS_AVFORMAT
/**
 * Find the encoder a muxer uses when the caller does not choose one.
 *
 * @param format  the muxer
 * @param isVideo `true` for the video codec, `false` for the audio one
 *
 * @return the default encoder, or an invalid codec when the muxer has none
 */
Codec findEncodingCodec(const OutputFormat &format, bool isVideo = true);
/**
 * Guess the encoder for an output from its hints.
 *
 * @param format    the muxer
 * @param name      file name that hints at the codec; may be empty
 * @param url       URL that hints at the codec; may be empty
 * @param mime      MIME type that hints at the codec; may be empty
 * @param mediaType media type to guess for
 *
 * @return the guessed encoder, or an invalid codec when nothing matches
 */
Codec guessEncodingCodec(OutputFormat format, const char *name, const char *url, const char* mime, AVMediaType mediaType);
#endif // if AVCPP_HAS_AVFORMAT

} // ::fmpeg
