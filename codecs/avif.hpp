#ifndef AVIF_HPP
#define AVIF_HPP

// TODO: libheif is supposed to support reading AVIF files. I have not yet found
// any AVIF files that it does work with, though. If libheif support improves in
// the future, we can retire this module and use the heif module instead
#include "image.hpp"

#include <algorithm>

inline bool is_avif(const Image::Header & header)
{
    const std::array<unsigned char, 4> ftyp_header = {'f', 't', 'y', 'p'};
    const std::array<std::array<unsigned char, 4>, 2> brand = {{
        {'a', 'v', 'i', 'f'},
        {'a', 'v', 'i', 's'},
    }};

    if(!std::equal(std::begin(ftyp_header), std::end(ftyp_header), std::begin(header) + 4, Image::header_cmp))
        return false;

    return std::any_of(std::begin(brand), std::end(brand), [&header](const auto & b)
            {
                return std::equal(std::begin(b), std::end(b), std::begin(header) + 8, Image::header_cmp);
            });
}

#ifdef AVIF_FOUND
class Avif final: public Image
{
public:
    explicit Avif(std::istream & input);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // AVIF_HPP
