#ifndef EXIF_HPP
#define EXIF_HPP

#include <cstddef>

#include "config.h"

namespace exif
{
    enum class Orientation:short { r_0=1, r_180=3, r_270=6, r_90=8 };
#ifdef EXIF_FOUND
    Orientation get_orientation(const unsigned char * data , std::size_t len);
#endif // EXIF_FOUND
}

#endif // EXIF_HPP
