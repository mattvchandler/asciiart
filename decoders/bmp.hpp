#ifndef BMP_HPP
#define BMP_HPP

#include <istream>
#include <vector>

#include "image.hpp"

inline bool is_bmp(const Image::Header & header)
{
    const std::array<unsigned char, 2> bmp_header {0x42, 0x4D};
    return std::equal(std::begin(bmp_header), std::end(bmp_header), std::begin(header), Image::header_cmp);
}

class Bmp final: public Image
{
public:
    Bmp(const Header & header, std::istream & input, int bg);

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
#endif // BMP_HPP
