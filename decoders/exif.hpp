#ifndef EXIF_HPP
#define EXIF_HPP

#include "config.h"

#include <optional>

namespace exif
{
    enum class Orientation:short { r_0=1, r_180=3, r_270=6, r_90=8 };
#ifdef EXIF_FOUND
    std::optional<Orientation> get_orientation(const unsigned char * data , std::size_t len);
#endif // EXIF_FOUND
}

#endif // EXIF_HPP
