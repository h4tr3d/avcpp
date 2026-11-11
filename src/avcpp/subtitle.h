#pragma once

#include <string_view>
#include <vector>

#include "avcompat.h"
#include "averror.h"
#include "ffmpeg.h"
#include "rational.h"
#include "timestamp.h"

#if AVCPP_CXX_STANDARD >= 20
#include <span>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace av {

namespace subtitle {

/**
 * Returns a built-in ASS/SSA script header accepted by the FFmpeg text subtitle
 * encoders (subrip, webvtt, mov_text, text and ass).
 *
 * The value does not depend on the FFmpeg version, unlike the headers generated
 * by the FFmpeg subtitle decoders.
 */
std::string_view defaultAssHeader() noexcept;

} // namespace subtitle

/**
 * Non-owning view over AVSubtitleRect.
 *
 * The view is valid while the owning SubtitleFrame is alive. Setters are meant
 * for rectangles owned by a SubtitleFrame; they allocate memory compatible with
 * avsubtitle_free().
 */
class SubtitleRect : public FFWrapperPtr<AVSubtitleRect>
{
public:
    SubtitleRect() = default;
    explicit SubtitleRect(AVSubtitleRect *rect);

    bool isValid() const noexcept;
    operator bool() const noexcept;

    AVSubtitleType type() const noexcept;
    void setType(AVSubtitleType type) noexcept;

    int x() const noexcept;
    int y() const noexcept;
    int width() const noexcept;
    int height() const noexcept;
    void setArea(int x, int y, int width, int height) noexcept;

    int nbColors() const noexcept;

    /**
     * Plain UTF-8 text. Empty when the rectangle has no text.
     */
    std::string_view text() const noexcept;
    /**
     * ASS/SSA event line. Empty when the rectangle has no ASS payload.
     */
    std::string_view ass() const noexcept;

    void setText(std::string_view text, OptionalErrorCode ec = throws());
    void setAss(std::string_view ass, OptionalErrorCode ec = throws());
    void clearText() noexcept;
    void clearAss() noexcept;

    bool isForced() const noexcept;
    void setForced(bool forced) noexcept;

    uint8_t *data(size_t plane = 0) noexcept;
    const uint8_t *data(size_t plane = 0) const noexcept;
    int linesize(size_t plane = 0) const noexcept;

    /**
     * Sets plane 0 to an index bitmap and updates width, height and linesize.
     * Also sets the rectangle type to SUBTITLE_BITMAP when it was SUBTITLE_NONE.
     */
    void setBitmap(const uint8_t *data, int stride, int width, int height,
                   OptionalErrorCode ec = throws());
    /**
     * Sets plane 1 to a palette and updates nb_colors.
     */
    void setPalette(const uint32_t *palette, int count, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Plane 0 as an index bitmap: linesize(0) * height() bytes.
     */
    std::span<const uint8_t> bitmap() const noexcept;
    /**
     * Plane 1 as a palette: nbColors() entries.
     */
    std::span<const uint32_t> palette() const noexcept;

    void setBitmap(std::span<const uint8_t> data, int stride, int width, int height,
                   OptionalErrorCode ec = throws());
    void setPalette(std::span<const uint32_t> palette, OptionalErrorCode ec = throws());
#endif
};

/**
 * Owning, move-only subtitle frame.
 *
 * Wraps AVSubtitle. AVSubtitle has no reference counting API in FFmpeg, so
 * copies are disabled; use clone() for an explicit deep copy.
 *
 * The pts value is interpreted in AV_TIME_BASE units, as required by FFmpeg.
 * The default time base is 1/AV_TIME_BASE; setTimeBase() rescales the stored
 * pts value.
 */
class SubtitleFrame
{
public:
    SubtitleFrame() noexcept;
    SubtitleFrame(SubtitleFrame &&other) noexcept;
    SubtitleFrame &operator=(SubtitleFrame &&other) noexcept;
    SubtitleFrame(const SubtitleFrame &) = delete;
    SubtitleFrame &operator=(const SubtitleFrame &) = delete;
    ~SubtitleFrame();

    /**
     * Deep copy: strings, bitmap (linesize(0) * height) and palette are copied.
     * Planes 2 and 3 are ignored because FFmpeg subtitle codecs do not use them.
     */
    SubtitleFrame clone(OptionalErrorCode ec = throws()) const;

    AVSubtitle *raw() noexcept;
    const AVSubtitle *raw() const noexcept;

    /**
     * True when the frame contains rectangles or is a decoded empty subtitle
     * (for example, a bitmap subtitle that clears the screen).
     */
    bool isValid() const noexcept;
    operator bool() const noexcept;

    /**
     * 0 for graphics, non-zero for text.
     */
    uint16_t format() const noexcept;
    void setFormat(uint16_t format) noexcept;
    bool isText() const noexcept;

    uint32_t startDisplayTime() const noexcept;
    uint32_t endDisplayTime() const noexcept;
    void setStartDisplayTime(uint32_t ms) noexcept;
    void setEndDisplayTime(uint32_t ms) noexcept;

    Timestamp pts() const noexcept;
    void setPts(const Timestamp &ts);
    const Rational &timeBase() const noexcept;
    void setTimeBase(const Rational &value) noexcept;

    /**
     * pts + startDisplayTime(), in the frame time base.
     */
    Timestamp startTime() const noexcept;
    /**
     * pts + endDisplayTime(), in the frame time base.
     */
    Timestamp endTime() const noexcept;

    int streamIndex() const noexcept;
    void setStreamIndex(int index) noexcept;

    /**
     * True when a decoder produced this frame, including empty subtitles.
     */
    bool isComplete() const noexcept;
    void setComplete(bool complete) noexcept;

    size_t numRects() const noexcept;
    SubtitleRect rect(size_t index) const;
    std::vector<SubtitleRect> rects() const;

    /**
     * Appends a new empty rectangle owned by this frame.
     */
    SubtitleRect addRect(AVSubtitleType type = SUBTITLE_NONE,
                         OptionalErrorCode ec = throws());
    /**
     * Releases all rectangles and resets the frame state.
     */
    void clear() noexcept;

    std::string_view text() const noexcept;
    std::string_view ass() const noexcept;

    static SubtitleFrame fromAss(std::string_view ass, OptionalErrorCode ec = throws());
    static SubtitleFrame fromText(std::string_view text, OptionalErrorCode ec = throws());

private:
    void swap(SubtitleFrame &other) noexcept;

    AVSubtitle m_raw{};
    Rational m_timeBase{1, AV_TIME_BASE};
    int m_streamIndex{-1};
    bool m_isComplete{false};
};

} // namespace av
