#ifndef WEBP_HPP
#define WEBP_HPP

#include "image.hpp"

inline bool is_webp(const Image::Header & header)
{
    const std::array<unsigned char, 4> webp_header1 = {0x52, 0x49, 0x46, 0x46};
    // there are 4 don't care bytes in between
    const std::array<unsigned char, 4> webp_header2 = {0x57, 0x45, 0x42, 0x50};

    return std::equal(std::begin(webp_header1), std::end(webp_header1), std::begin(header), Image::header_cmp) &&
           std::equal(std::begin(webp_header2), std::end(webp_header2), std::begin(header) + 8, Image::header_cmp);
}

#ifdef WEBP_FOUND
class Webp final: public Image
{
public:
    explicit Webp(std::istream & input);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // WEBP_HPP
