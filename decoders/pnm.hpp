#ifndef PNM_HPP
#define PNM_HPP

#include "image.hpp"

#include <istream>
#include <vector>

class Pnm final: public Image
{
public:
    Pnm(const Header & header, std::istream & input);
    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    };

    std::size_t get_width() const override { return width_; };
    std::size_t get_height() const override { return height_; };
private:
    std::size_t width_{0};
    std::size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;

    void read_P1(std::istream & input);
    void read_P2(std::istream & input);
    void read_P3(std::istream & input);
    void read_P4(std::istream & input);
    void read_P5(std::istream & input);
    void read_P6(std::istream & input);
};
#endif // PNM_HPP
