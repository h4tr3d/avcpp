#pragma once

/**
 * Rectangles and geometry helpers.
 *
 * @file
 * @ingroup core
 */

namespace av {

/**
 * Rectangle: a position and a size.
 *
 * Used where FFmpeg deals in areas rather than points, for example a subtitle
 * bitmap or a crop window.
 */
class Rect
{
public:
    /**
     * Create a zero-sized rectangle at the origin.
     */
    Rect() noexcept = default;
    /**
     * Create a rectangle at the origin.
     *
     * @param width  width in pixels
     * @param height height in pixels
     */
    Rect(int width, int height) noexcept;
    /**
     * Create a rectangle at a position.
     *
     * @param x      left edge in pixels
     * @param y      top edge in pixels
     * @param width  width in pixels
     * @param height height in pixels
     */
    Rect(int x, int y, int width, int height) noexcept;

    /**
     * Set the left edge.
     *
     * @param x position in pixels
     */
    void setX(int x) noexcept { this->x = x; }
    /**
     * Set the top edge.
     *
     * @param y position in pixels
     */
    void setY(int y) noexcept { this->y = y; }
    /**
     * Set the width.
     *
     * @param w width in pixels
     */
    void setWidth(int w) noexcept { width = w; }
    /**
     * Set the height.
     *
     * @param h height in pixels
     */
    void setHeight(int h) noexcept { height = h; }

    /**
     * The left edge.
     *
     * @return position in pixels
     */
    int getX() const noexcept { return x; }
    /**
     * The top edge.
     *
     * @return position in pixels
     */
    int getY() const noexcept { return y; }
    /**
     * The width.
     *
     * @return width in pixels
     */
    int getWidth() const noexcept { return width; }
    /**
     * The height.
     *
     * @return height in pixels
     */
    int getHeight() const noexcept { return height; }

private:
    int x{};
    int y{};
    int width{};
    int height{};
};

} // ::av

#if __has_include(<format>)
#include <format>
#ifdef __cpp_lib_format
// std::format
template <typename CharT>
struct std::formatter<av::Rect, CharT>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template<typename ParseContext>
    auto format(const av::Rect& value, ParseContext& ctx) const
    {
        return std::format_to(ctx.out(), "{}x{}{:+}{:+}", value.getWidth(), value.getHeight(), value.getX(), value.getY());
    }
};
#endif // __cpp_lib_format
#endif // __has_include
