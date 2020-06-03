#ifndef FLIF_HPP
#define FLIF_HPP

#include "image.hpp"

#include <algorithm>

inline bool is_flif(const Image::Header & header)
{
    const std::array<unsigned char, 4> flif_header = {'F', 'L', 'I', 'F'};
    return std::equal(std::begin(flif_header), std::end(flif_header), std::begin(header), Image::header_cmp);
}

#ifdef FLIF_FOUND
class Flif final: public Image
{
public:
    explicit Flif(std::istream & input);
};
#endif
#endif // FLIF_HPP
