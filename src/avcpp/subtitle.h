#pragma once

/**
 * Subtitle frames: text and ASS payloads, bitmap rectangles.
 *
 * @file
 * @ingroup subtitle
 */

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
 * @return the header text, valid for the lifetime of the program
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
        /**
         * Wrap a rectangle.
         *
         * @param rect the FFmpeg rectangle; null produces an invalid view
         *
         * @note Non-owning: the rectangle must outlive the view.
         */
    explicit SubtitleRect(AVSubtitleRect *rect);

        /**
         * Whether the view refers to a rectangle.
         *
         * @return `true` when the underlying AVSubtitleRect exists
         */
    bool isValid() const noexcept;
        /**
         * Whether the view refers to a rectangle.
         *
         * @return `true` when the underlying AVSubtitleRect exists
         */
    operator bool() const noexcept;

        /**
         * Kind of payload the rectangle carries.
         *
         * @return `SUBTITLE_TEXT`, `SUBTITLE_ASS`, `SUBTITLE_BITMAP` or
         *         `SUBTITLE_NONE`
         */
    AVSubtitleType type() const noexcept;
        /**
         * Set the payload kind.
         *
         * @param type the `AVSubtitleType` to record
         */
    void setType(AVSubtitleType type) noexcept;

        /**
         * Horizontal position of the rectangle.
         *
         * @return the left edge in pixels
         */
    int x() const noexcept;
        /**
         * Vertical position of the rectangle.
         *
         * @return the top edge in pixels
         */
    int y() const noexcept;
        /**
         * Width of the bitmap.
         *
         * @return the width in pixels
         */
    int width() const noexcept;
        /**
         * Height of the bitmap.
         *
         * @return the height in pixels
         */
    int height() const noexcept;
        /**
         * Set the position and the size at once.
         *
         * @param x      left edge in pixels
         * @param y      top edge in pixels
         * @param width  width in pixels
         * @param height height in pixels
         */
    void setArea(int x, int y, int width, int height) noexcept;

        /**
         * Number of palette entries of a bitmap rectangle.
         *
         * @return the entry count, or zero when there is no palette
         */
    int nbColors() const noexcept;

    /**
     * Plain UTF-8 text. Empty when the rectangle has no text.
     * @return the text, empty when there is none
     */
    std::string_view text() const noexcept;
    /**
     * ASS/SSA event line. Empty when the rectangle has no ASS payload.
     * @return the ASS line, empty when there is no payload
     */
    std::string_view ass() const noexcept;

        /**
         * Store plain UTF-8 text in the rectangle.
         *
         * The previous payload is freed. Memory comes from the FFmpeg allocator, so
         * it is released by avsubtitle_free() together with the frame.
         *
         * @param text the text to store
         * @param ec   holder for the error code
         */
    void setText(std::string_view text, OptionalErrorCode ec = throws());
        /**
         * Store an ASS/SSA event line in the rectangle.
         *
         * @param ass the dialogue line, without the "Dialogue:" prefix
         * @param ec  holder for the error code
         */
    void setAss(std::string_view ass, OptionalErrorCode ec = throws());
        /**
         * Free the text payload and clear the pointer.
         */
    void clearText() noexcept;
        /**
         * Free the ASS payload and clear the pointer.
         */
    void clearAss() noexcept;

        /**
         * Whether the rectangle is flagged as forced.
         *
         * Forced subtitles are shown even when the user disabled subtitles, which
         * is how translations of on-screen text are usually delivered.
         *
         * @return the forced flag
         */
    bool isForced() const noexcept;
        /**
         * Mark the rectangle as forced.
         *
         * @param forced the new value of the flag
         */
    void setForced(bool forced) noexcept;

        /**
         * Pointer to one image plane.
         *
         * @param plane plane index: 0 is the bitmap, 1 is the palette
         *
         * @return the plane pointer, or nullptr when it is absent
         *
         * @note Borrowed; valid while the owning frame lives.
         */
    uint8_t *data(size_t plane = 0) noexcept;
        /**
         * Pointer to one image plane, read-only.
         *
         * @param plane plane index
         *
         * @return the plane pointer, or nullptr when it is absent
         */
    const uint8_t *data(size_t plane = 0) const noexcept;
        /**
         * Distance between two rows of a plane.
         *
         * @param plane plane index
         *
         * @return the stride in bytes
         */
    int linesize(size_t plane = 0) const noexcept;

    /**
     * Sets plane 0 to an index bitmap and updates width, height and linesize.
     * Also sets the rectangle type to SUBTITLE_BITMAP when it was SUBTITLE_NONE.
     * @param data
     * @param stride
     * @param width
     * @param height
     * @param ec
     */
    void setBitmap(const uint8_t *data, int stride, int width, int height,
                   OptionalErrorCode ec = throws());
    /**
     * Sets plane 1 to a palette and updates nb_colors.
     * @param palette
     * @param count
     * @param ec
     */
    void setPalette(const uint32_t *palette, int count, OptionalErrorCode ec = throws());

#if AVCPP_CXX_STANDARD >= 20
    /**
     * Plane 0 as an index bitmap: linesize(0) * height() bytes.
     * @return the bitmap bytes
     */
    std::span<const uint8_t> bitmap() const noexcept;
    /**
     * Plane 1 as a palette: nbColors() entries.
     * @return the palette entries
     */
    std::span<const uint32_t> palette() const noexcept;

        /**
         * Sets plane 0 to an index bitmap and updates width, height and linesize.
         *
         * @param data   the bitmap bytes
         * @param stride bytes per row
         * @param width  width in pixels
         * @param height height in pixels
         * @param ec     holder for the error code
         */
    void setBitmap(std::span<const uint8_t> data, int stride, int width, int height,
                   OptionalErrorCode ec = throws());
        /**
         * Sets plane 1 to a palette and updates nb_colors.
         *
         * @param palette the palette entries
         * @param ec      holder for the error code
         */
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
        /**
         * Move the frame.
         *
         * @param other the frame to move from; left empty
         */
    SubtitleFrame(SubtitleFrame &&other) noexcept;
        /**
         * Move assignment: frees the current content and takes the other frame's.
         *
         * @param other the frame to move from; left empty
         *
         * @return this frame
         */
    SubtitleFrame &operator=(SubtitleFrame &&other) noexcept;
    SubtitleFrame(const SubtitleFrame &) = delete;
    SubtitleFrame &operator=(const SubtitleFrame &) = delete;
    ~SubtitleFrame();

    /**
     * Deep copy: strings, bitmap (linesize(0) * height) and palette are copied.
     * Planes 2 and 3 are ignored because FFmpeg subtitle codecs do not use them.
     * @param ec
     * @return an independent copy of the frame
     */
    SubtitleFrame clone(OptionalErrorCode ec = throws()) const;

        /**
         * The underlying FFmpeg subtitle.
         *
         * @return the raw structure, never null
         *
         * @note Borrowed. Do not free it; the frame owns it.
         */
    AVSubtitle *raw() noexcept;
        /**
         * The underlying FFmpeg subtitle, read-only.
         *
         * @return the raw structure, never null
         */
    const AVSubtitle *raw() const noexcept;

    /**
     * True when the frame contains rectangles or is a decoded empty subtitle
     * (for example, a bitmap subtitle that clears the screen).
     * @return `true` when the frame is usable
     */
    bool isValid() const noexcept;
        /**
         * Whether the frame holds a subtitle.
         *
         * @return `true` when the frame is valid, including empty subtitles that
         *         clear the screen
         */
    operator bool() const noexcept;

    /**
     * 0 for graphics, non-zero for text.
     * @return zero for bitmap subtitles, non-zero for text
     */
    uint16_t format() const noexcept;
        /**
         * Set the subtitle kind.
         *
         * @param format zero for bitmap subtitles, non-zero for text
         */
    void setFormat(uint16_t format) noexcept;
        /**
         * Whether the subtitle is textual.
         *
         * @return `true` when format() is non-zero
         */
    bool isText() const noexcept;

        /**
         * Start of the display interval, relative to pts().
         *
         * @return milliseconds from pts() until the subtitle appears
         */
    uint32_t startDisplayTime() const noexcept;
        /**
         * End of the display interval, relative to pts().
         *
         * @return milliseconds from pts() until the subtitle disappears
         */
    uint32_t endDisplayTime() const noexcept;
        /**
         * Set the start of the display interval.
         *
         * @param ms milliseconds from pts() until the subtitle appears
         */
    void setStartDisplayTime(uint32_t ms) noexcept;
        /**
         * Set the end of the display interval.
         *
         * @param ms milliseconds from pts() until the subtitle disappears
         */
    void setEndDisplayTime(uint32_t ms) noexcept;

        /**
         * Presentation timestamp of the subtitle.
         *
         * @return the timestamp in timeBase() units
         */
    Timestamp pts() const noexcept;
        /**
         * Set the presentation timestamp.
         *
         * @param ts the timestamp; its time base replaces the stored one
         */
    void setPts(const Timestamp &ts);
        /**
         * Time base of pts().
         *
         * @return the time base; it defaults to 1/AV_TIME_BASE
         */
    const Rational &timeBase() const noexcept;
        /**
         * Set the time base, rescaling the stored timestamp.
         *
         * @param value timestamp units per second
         */
    void setTimeBase(const Rational &value) noexcept;

    /**
     * pts + startDisplayTime(), in the frame time base.
     * @return the absolute start of the display interval
     */
    Timestamp startTime() const noexcept;
    /**
     * pts + endDisplayTime(), in the frame time base.
     * @return the absolute end of the display interval
     */
    Timestamp endTime() const noexcept;

        /**
         * Index of the stream the subtitle came from.
         *
         * @return the stream index, or -1 when it has not been set
         */
    int streamIndex() const noexcept;
        /**
         * Set the stream index.
         *
         * @param index index of the subtitle stream in the container
         */
    void setStreamIndex(int index) noexcept;

    /**
     * True when a decoder produced this frame, including empty subtitles.
     * @return the completeness flag
     */
    bool isComplete() const noexcept;
        /**
         * Mark the frame as produced by a decoder.
         *
         * @param complete `true` for a decoded subtitle, including an empty one
         */
    void setComplete(bool complete) noexcept;

        /**
         * Number of rectangles in the frame.
         *
         * @return the rectangle count
         */
    size_t numRects() const noexcept;
        /**
         * Rectangle by index.
         *
         * @param index zero-based index; must be less than numRects()
         *
         * @return a non-owning view of the rectangle
         */
    SubtitleRect rect(size_t index) const;
        /**
         * All rectangles as a vector of views.
         *
         * @return the views; they share the frame's lifetime
         */
    std::vector<SubtitleRect> rects() const;

    /**
     * Appends a new empty rectangle owned by this frame.
     * @param type
     * @param ec
     * @return a view of the new rectangle
     */
    SubtitleRect addRect(AVSubtitleType type = SUBTITLE_NONE,
                         OptionalErrorCode ec = throws());
    /**
     * Releases all rectangles and resets the frame state.
     */
        /**
         * Free all rectangles and reset the frame state.
         */
    void clear() noexcept;

        /**
         * Text of the subtitle: the payload of the first text rectangle.
         *
         * @return the text, empty when the frame has none
         */
    std::string_view text() const noexcept;
        /**
         * ASS line of the subtitle: the payload of the first ASS rectangle.
         *
         * @return the line, empty when the frame has none
         */
    std::string_view ass() const noexcept;

        /**
         * Build a frame from one ASS dialogue line.
         *
         * The result contains a single SUBTITLE_ASS rectangle with a zero start
         * display time, which is what text subtitle encoders accept.
         *
         * @param ass the dialogue line, without the "Dialogue:" prefix
         * @param ec  holder for the error code
         *
         * @return the new frame, or an invalid one on failure
         */
    static SubtitleFrame fromAss(std::string_view ass, OptionalErrorCode ec = throws());
        /**
         * Build a frame from plain text.
         *
         * The result contains a single SUBTITLE_TEXT rectangle with a zero start
         * display time.
         *
         * @param text the text of the subtitle
         * @param ec   holder for the error code
         *
         * @return the new frame, or an invalid one on failure
         */
    static SubtitleFrame fromText(std::string_view text, OptionalErrorCode ec = throws());

private:
    void swap(SubtitleFrame &other) noexcept;

    AVSubtitle m_raw{};
    Rational m_timeBase{1, AV_TIME_BASE};
    int m_streamIndex{-1};
    bool m_isComplete{false};
};

} // namespace av
