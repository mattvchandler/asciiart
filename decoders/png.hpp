#ifndef PNG_HPP
#define PNG_HPP

#ifdef HAS_PNG
#include <istream>
#include <vector>

#include <png.h>
#endif

#include "image.hpp"

inline bool is_png(const Image::Header & header)
{
    const std::array<unsigned char, 8> png_header = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return std::equal(std::begin(png_header), std::end(png_header), std::begin(header), Image::header_cmp);
}

#ifdef HAS_PNG
class Png final: public Image
{
public:
    Png(const Header & header, std::istream & input, int bg);

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

    const Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};

    void read_fn(png_bytep data, png_size_t length) noexcept;
    static void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept;
};
#endif
#endif // PNG_HPP
