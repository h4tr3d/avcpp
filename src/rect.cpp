#include "rect.h"

namespace av {

Rect::Rect()
    : x(0), y(0), width(0), height(0)
{
}

Rect::Rect(int width, int height)
    : x(0), y(0), width(width), height(height)
{
}

Rect::Rect(int x, int y, int width, int height)
    : x(x), y(y), width(width), height(height)
{
}

} // ::av
