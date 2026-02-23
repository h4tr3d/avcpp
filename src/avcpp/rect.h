#pragma once

namespace av {

class Rect
{
public:
    Rect() noexcept = default;
    Rect(int width, int height) noexcept;
    Rect(int x, int y, int width, int height) noexcept;

    void setX(int x) noexcept { this->x = x; }
    void setY(int y) noexcept { this->y = y; }
    void setWidth(int w) noexcept { width = w; }
    void setHeight(int h) noexcept { height = h; }

    int getX() const noexcept { return x; }
    int getY() const noexcept { return y; }
    int getWidth() const noexcept { return width; }
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
