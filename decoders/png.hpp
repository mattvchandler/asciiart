#ifndef PNG_HPP
#define PNG_HPP

#ifdef HAS_PNG
#include "image.hpp"

#include <istream>
#include <vector>

#include <png.h>

class Png final: public Image
{
public:
    Png(const Header & header, std::istream & input, int bg);
    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    };

    std::size_t get_width() const override { return width_; };
    std::size_t get_height() const override { return height_; };
private:
    const Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};

    std::size_t width_{0};
    std::size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;

    void read_fn(png_bytep data, png_size_t length) noexcept;
    static void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept;
};
#endif
#endif // PNG_HPP
