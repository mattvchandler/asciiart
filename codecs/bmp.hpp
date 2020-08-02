#ifndef BMP_HPP
#define BMP_HPP

#include "image.hpp"

inline bool is_bmp(const Image::Header & header)
{
    const std::array<unsigned char, 2> bmp_header {0x42, 0x4D};
    return std::equal(std::begin(bmp_header), std::end(bmp_header), std::begin(header), Image::header_cmp);
}

class Bmp final: public Image
{
public:
    explicit Bmp(std::istream & input);
};
#endif // BMP_HPP
