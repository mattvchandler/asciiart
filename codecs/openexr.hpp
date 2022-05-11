#ifndef OPENEXR_HPP
#define OPENEXR_HPP

#include "image.hpp"

inline bool is_openexr(const Image::Header & header)
{
    const std::array<unsigned char, 4> exr_header = {0x76, 0x2F, 0x31, 0x01};

    return std::equal(std::begin(exr_header), std::end(exr_header), std::begin(header), Image::header_cmp);
}

#ifdef OpenEXR_FOUND
class OpenEXR final: public Image
{
public:
    OpenEXR() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // OPENEXR_HPP
