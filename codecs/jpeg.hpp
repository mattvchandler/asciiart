#ifndef JPEG_HPP
#define JPEG_HPP

#include "image.hpp"

inline bool is_jpeg(const Image::Header & header)
{
    const std::array<unsigned char, 4>  jpeg_header1   = {0xFF, 0xD8, 0xFF, 0xDB};
    const std::array<unsigned char, 12> jpeg_header2   = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01};
    const std::array<unsigned char, 4>  jpeg_header3   = {0xFF, 0xD8, 0xFF, 0xEE};
    const std::array<unsigned char, 4>  jpeg_header4_1 = {0xFF, 0xD8, 0xFF, 0xE1};
    const std::array<unsigned char, 6>  jpeg_header4_2 = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00}; // there are 2 "don't care" bytes between these

    return std::equal(std::begin(jpeg_header1),   std::end(jpeg_header1),   std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpeg_header2),   std::end(jpeg_header2),   std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), Image::header_cmp)
       || (std::equal(std::begin(jpeg_header4_1), std::end(jpeg_header4_1), std::begin(header), Image::header_cmp)
           && std::equal(std::begin(jpeg_header4_2), std::end(jpeg_header4_2), std::begin(header) + std::size(jpeg_header4_1) + 2, Image::header_cmp));
}

#ifdef JPEG_FOUND
class Jpeg final: public Image
{
public:
    Jpeg() = default;
    void open(std::istream & input, const Args & args) override;

    bool supports_multiple_images() const override { return true; }

    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif
#endif // JPEG_HPP
