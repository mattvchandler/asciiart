#ifndef JPEGXL_HPP
#define JPEGXL_HPP

#include "image.hpp"

inline bool is_jpegxl(const Image::Header & header)
{
    const std::array<unsigned char, 2>  jpegxl_header1 = {0xFF, 0x0A};
    const std::array<unsigned char, 6>  jpegxl_header2 = {0x0A, 0x04, 0x42, 0xD2, 0xD5, 0x4E};
    const std::array<unsigned char, 12> jpegxl_header3 = {0x00, 0x00, 0x00, 0x0C, 0x4A, 0x58, 0x4C, 0x20,
                                                          0x0D, 0x0A, 0x87, 0x0A};

    return std::equal(std::begin(jpegxl_header1), std::end(jpegxl_header1), std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpegxl_header2), std::end(jpegxl_header2), std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpegxl_header3), std::end(jpegxl_header3), std::begin(header), Image::header_cmp);
}

#ifdef JPEGXL_FOUND
class JpegXL final: public Image
{
public:
    explicit JpegXL(std::istream & input);
};
#endif
#endif // JPEGXL_HPP
