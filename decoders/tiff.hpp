#ifndef TIFF_HPP
#define TIFF_HPP

#ifdef HAS_TIFF
#include <istream>
#include <vector>
#endif

#include "image.hpp"

inline bool is_tiff(const Image::Header & header)
{
    const std::array<unsigned char, 4> tiff_header1 = {0x49, 0x49, 0x2A, 0x00};
    const std::array<unsigned char, 4> tiff_header2 = {0x4D, 0x4D, 0x00, 0x2A};

    return std::equal(std::begin(tiff_header1), std::end(tiff_header1), std::begin(header), Image::header_cmp) ||
           std::equal(std::begin(tiff_header2), std::end(tiff_header2), std::begin(header), Image::header_cmp);
}

#ifdef HAS_TIFF
class Tiff final: public Image
{
public:
    Tiff(const Header & header, std::istream & input, unsigned char bg);

    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    }

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }

private:
    std::size_t width_{0};
    std::size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;
};
#endif
#endif // TIFF_HPP
