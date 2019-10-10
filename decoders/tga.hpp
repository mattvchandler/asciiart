#ifndef TGA_HPP
#define TGA_HPP

#include <istream>
#include <vector>

#include "image.hpp"

class Tga final: public Image
{
public:
    Tga(const Header & header, std::istream & input, unsigned char bg);

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
#endif // TGA_HPP
