#ifndef FLIF_HPP
#define FLIF_HPP

#include "image.hpp"

#include <algorithm>

inline bool is_flif(const Image::Header & header)
{
    const std::array<unsigned char, 4> flif_header = {'F', 'L', 'I', 'F'};
    return std::equal(std::begin(flif_header), std::end(flif_header), std::begin(header), Image::header_cmp);
}

#if defined(FLIF_ENC_FOUND) || defined(FLIF_DEC_FOUND)
class Flif final: public Image
{
public:
    #ifdef FLIF_DEC_FOUND
    Flif(std::istream & input, const Args & args);
    #endif

    #ifdef FLIF_ENC_FOUND
    static void write(std::ostream & out, const Image & img, bool invert);
    #endif
};
#endif
#endif // FLIF_HPP
