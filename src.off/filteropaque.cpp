#include "filteropaque.h"

namespace av {



FilterOpaque::FilterOpaque()
    : opaque(0)
{
}

FilterOpaque::~FilterOpaque()
{
}

void *FilterOpaque::getOpaque() const
{
    return opaque;
}

void FilterOpaque::setOpaque(void *ptr)
{
    opaque = ptr;
}

} // ::av
