#include "rect.h"

namespace av {

Rect::Rect(int width, int height) noexcept
    : width(width),
      height(height)
{
}

Rect::Rect(int x, int y, int width, int height) noexcept
    : x(x),
      y(y),
      width(width),
      height(height)
{
}

} // ::av
