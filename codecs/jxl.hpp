#ifndef JXL_HPP
#define JXL_HPP

#include "image.hpp"

inline bool is_jxl(const Image::Header & header)
{
    const std::array<unsigned char, 2>  jxl_header1 = {0xFF, 0x0A};
    const std::array<unsigned char, 6>  jxl_header2 = {0x0A, 0x04, 0x42, 0xD2, 0xD5, 0x4E};
    const std::array<unsigned char, 12> jxl_header3 = {0x00, 0x00, 0x00, 0x0C, 0x4A, 0x58, 0x4C, 0x20,
                                                       0x0D, 0x0A, 0x87, 0x0A};

    return std::equal(std::begin(jxl_header1), std::end(jxl_header1), std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jxl_header2), std::end(jxl_header2), std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jxl_header3), std::end(jxl_header3), std::begin(header), Image::header_cmp);
}

#ifdef JXL_FOUND
class Jxl final: public Image
{
public:
    Jxl() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // JXL_HPP
