#ifndef XPM_HPP
#define XPM_HPP

#include <istream>
#include <vector>

#include "image.hpp"

class Xpm final: public Image
{
public:
    Xpm(const Header & header, std::istream & input, int bg);

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
#endif // XPM_HPP
