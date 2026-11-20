#pragma once

/**
 * Input and output format descriptors.
 *
 * @file
 * @ingroup avformat
 */

#include <memory>
#include <string>

#include "ffmpeg.h"

#if AVCPP_HAS_AVFORMAT

extern "C" {
#include "libavformat/avformat.h"
}

namespace av {

// AVFormat 59 will introduce `const` for all muxer/demuxer description operations
/**
 * Const or non-const FFmpeg format type, depending on the FFmpeg version.
 *
 * FFmpeg 59 made the muxer/demuxer descriptors const; this alias keeps one
 * code path compiling against both.
 *
 * @tparam T AVFormat type to qualify
 */
template<typename T>
using avcpp_format_const = typename std::conditional<
    std::is_const<
        typename std::remove_pointer<
            decltype(AVFormatContext::iformat)
        >::type
    >::value, const T, T>::type;

/// Forward declaration of av::Codec, so this header need not include it.
using FmtCodec = class Codec;
namespace internal {
/**
 * Whether a codec tag table can carry a codec.
 *
 * @param codecTag the muxer's tag table
 * @param codec    the codec to test
 *
 * @return `true` when the table has a tag for the codec
 */
bool codec_supported(const AVCodecTag *const *codecTag, const FmtCodec &codec);
/**
 * Whether a codec tag table can carry a codec identifier.
 *
 * @param codecTag the muxer's tag table
 * @param codec_id the `AV_CODEC_ID_*` value to test
 *
 * @return `true` when the table has a tag for the codec
 */
bool codec_supported(const AVCodecTag *const *codecTag, AVCodecID codec_id);
} // ::internal

/**
 * Common interface of input and output format descriptors.
 *
 * The template is instantiated with the const or non-const FFmpeg type,
 * depending on how old the FFmpeg headers are, so the same code compiles
 * against both.
 *
 * @tparam T AVInputFormat or AVOutputFormat, possibly const
 */
template<typename T>
struct Format : public FFWrapperPtr<T>
{
    using FFWrapperPtr<T>::FFWrapperPtr;

    /**
     * Short format name, as FFmpeg spells it.
     *
     * @return a static string such as "mp4", or nullptr when the format is not
     *         set
     */
    const char* name() const noexcept {
        return RAW_GET(name, nullptr);
    }

    /**
     * Human-readable format name.
     *
     * @return a static description, or nullptr when the format is not set
     */
    const char* longName() const noexcept {
        return RAW_GET(long_name, nullptr);
    }

    /**
     * Raw `AVFMT_*` flags of the format.
     *
     * @return the flag mask, or zero when the format is not set
     */
    int32_t flags() const noexcept {
        return RAW_GET(flags, 0);
    }

    /**
     * Whether all given flags are set.
     *
     * @param flags the `AVFMT_*` bits to test
     *
     * @return `true` when every bit is present
     */
    bool isFlags(int32_t flags) const noexcept {
        if (m_raw) {
            return (m_raw->flags & flags);
        }
        return false;
    }

    /**
     * Point the descriptor at another format.
     *
     * @param format the format to wrap; the wrapper does not own it
     */
    void setFormat(T* format) noexcept {
        FFWrapperPtr<T>::reset(format);
    }

    /**
     * Whether the format can carry a codec.
     *
     * @param codec the codec to test
     *
     * @return `true` when the muxer or demuxer knows the codec's tag
     */
    bool codecSupported(const class Codec &codec) const noexcept
    {
        if (!m_raw)
            return false;
        return internal::codec_supported(m_raw->codec_tag, codec);
    }

    /**
     * Whether the format can carry a codec identifier.
     *
     * @param codec_id the `AV_CODEC_ID_*` value to test
     *
     * @return `true` when the muxer or demuxer knows a tag for it
     */
    bool codecSupported(AVCodecID codec_id) const noexcept
    {
        if (!m_raw)
            return false;
        return internal::codec_supported(m_raw->codec_tag, codec_id);
    }

protected:
    /// The wrapped FFmpeg format; owned by the library, not by the wrapper.
    using FFWrapperPtr<T>::m_raw;
};

/**
 * Demuxer descriptor.
 *
 * Wraps the FFmpeg structure that describes a container reader; the library
 * owns it, the wrapper only points at it.
 */
class InputFormat : public Format<avcpp_format_const<AVInputFormat>>
{
public:
    using Format<avcpp_format_const<AVInputFormat>>::Format;
    using Format<avcpp_format_const<AVInputFormat>>::setFormat;

    InputFormat() = default;

    /**
     * Look a demuxer up by name.
     *
     * @param name the demuxer name, for example "matroska"
     */
    InputFormat(const std::string& name) noexcept;

    /**
     * Look a demuxer up by name and point the descriptor at it.
     *
     * @param name the demuxer name
     *
     * @return `true` when the name was found
     */
    bool setFormat(const std::string& name) noexcept;

};


/**
 * Muxer descriptor.
 *
 * Wraps the FFmpeg structure that describes a container writer. Besides the
 * lookup, it reports the codecs the muxer falls back to when the caller does
 * not choose one.
 */
class OutputFormat : public Format<avcpp_format_const<AVOutputFormat>>
{
public:
    using Format<avcpp_format_const<AVOutputFormat>>::Format;
    using Format<avcpp_format_const<AVOutputFormat>>::setFormat;

    OutputFormat() = default;

    /**
     * Look a muxer up by name, URL or MIME type.
     *
     * @param name muxer name, for example "mp4"
     * @param url  URL that hints at the format; may be empty
     * @param mime MIME type that hints at the format; may be empty
     */
    OutputFormat(const std::string& name,
                 const std::string& url  = std::string(),
                 const std::string& mime = std::string()) noexcept;

    /**
     * Look a muxer up and point the descriptor at it.
     *
     * @param name muxer name
     * @param url  URL that hints at the format; may be empty
     * @param mime MIME type that hints at the format; may be empty
     *
     * @return `true` when a muxer was found
     */
    bool        setFormat(const std::string& name,
                          const std::string& url  = std::string(),
                          const std::string& mime = std::string()) noexcept;

    /**
     * Video codec the muxer uses when the caller does not choose one.
     *
     * @return the `AV_CODEC_ID_*` value, or `AV_CODEC_ID_NONE`
     */
    AVCodecID   defaultVideoCodecId() const noexcept;
    /**
     * Audio codec the muxer uses when the caller does not choose one.
     *
     * @return the `AV_CODEC_ID_*` value, or `AV_CODEC_ID_NONE`
     */
    AVCodecID   defaultAudioCodecId() const noexcept;
};


/**
 * Find the muxer FFmpeg would pick for this output.
 *
 * @param name file name or muxer name
 * @param url  URL that hints at the format; may be empty
 * @param mime MIME type that hints at the format; may be empty
 *
 * @return the muxer, or an empty format when nothing matches
 */
OutputFormat guessOutputFormat(const std::string& name,
                               const std::string& url  = std::string(),
                               const std::string& mime = std::string());

} // namespace av

#endif // if AVCPP_HAS_AVFORMAT
