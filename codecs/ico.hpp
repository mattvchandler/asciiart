#ifndef ICO_HPP
#define ICO_HPP

#include "image.hpp"

inline bool is_ico(const Image::Header & header)
{
    const std::array<unsigned char, 4> ico_header {0x00, 0x00, 0x01, 0x00};
    const std::array<unsigned char, 4> cur_header {0x00, 0x00, 0x02, 0x00};
    return std::equal(std::begin(ico_header), std::end(ico_header), std::begin(header), Image::header_cmp) ||
           std::equal(std::begin(cur_header), std::end(cur_header), std::begin(header), Image::header_cmp);
}

class Ico final: public Image
{
public:
    Ico(std::istream & input, const Args & args);

    static void write_cur(std::ostream & out, const Image & img, bool invert);
    static void write_ico(std::ostream & out, const Image & img, bool invert);
};
#endif // ICO_HPP
