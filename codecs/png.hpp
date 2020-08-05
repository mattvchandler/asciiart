#ifndef PNG_HPP
#define PNG_HPP

#include "image.hpp"

inline bool is_png(const Image::Header & header)
{
    const std::array<unsigned char, 8> png_header = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return std::equal(std::begin(png_header), std::end(png_header), std::begin(header), Image::header_cmp);
}

#ifdef PNG_FOUND
class Png final: public Image
{
public:
    explicit Png(std::istream & input);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // PNG_HPP
