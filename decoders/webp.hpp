#ifndef WEBP_HPP
#define WEBP_HPP

#ifdef HAS_WEBP
#include <istream>
#include <vector>
#endif

#include "image.hpp"

inline bool is_webp(const Image::Header & header)
{
    const std::array<unsigned char, 4> webp_header1 = {0x52, 0x49, 0x46, 0x46};
    // there are 4 don't care bytes in between
    const std::array<unsigned char, 4> webp_header2 = {0x57, 0x45, 0x42, 0x50};

    return std::equal(std::begin(webp_header1), std::end(webp_header1), std::begin(header), Image::header_cmp) &&
           std::equal(std::begin(webp_header2), std::end(webp_header2), std::begin(header) + 8, Image::header_cmp);
}

#ifdef HAS_WEBP
class Webp final: public Image
{
public:
    Webp(const Header & header, std::istream & input, unsigned char bg);

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
#endif // WEBP_HPP
