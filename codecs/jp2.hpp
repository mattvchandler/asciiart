#ifndef JP2_HPP
#define JP2_HPP

#include "image.hpp"

#include <algorithm>

inline bool is_jp2(const Image::Header & header)
{
    const std::array<unsigned char, 12> jp2_header = {0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A};
    const std::array<unsigned char, 4> old_jp2_header = {0x0D, 0x0A, 0x87, 0x0A};
    return std::equal(std::begin(jp2_header), std::end(jp2_header), std::begin(header), Image::header_cmp) ||
           std::equal(std::begin(old_jp2_header), std::end(old_jp2_header), std::begin(header), Image::header_cmp);
}
inline bool is_jpx(const Image::Header & header)
{
    const std::array<unsigned char, 4> jpx_header = {0xFF, 0x4F, 0xFF, 0x51};
    return std::equal(std::begin(jpx_header), std::end(jpx_header), std::begin(header), Image::header_cmp);
}

#ifdef JP2_FOUND
class Jp2 final: public Image
{
public:
    enum class Type {JP2, JPX, JPT};
    Jp2(std::istream & input, Type type, const Args & args);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // JP2_HPP
