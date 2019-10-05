#ifndef PNM_HPP
#define PNM_HPP

#include <istream>
#include <vector>

#include "image.hpp"

inline bool is_pnm(const Image::Header & header)
{
    const std::array<unsigned char, 2> pbm_header   {0x50, 0x31};
    const std::array<unsigned char, 2> pgm_header   {0x50, 0x32};
    const std::array<unsigned char, 2> ppm_header   {0x50, 0x33};
    const std::array<unsigned char, 2> pbm_b_header {0x50, 0x34};
    const std::array<unsigned char, 2> pgm_b_header {0x50, 0x35};
    const std::array<unsigned char, 2> ppm_b_header {0x50, 0x36};

    return std::equal(std::begin(pbm_header),   std::end(pbm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pgm_header),   std::end(pgm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(ppm_header),   std::end(ppm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pbm_b_header), std::end(pbm_b_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pgm_b_header), std::end(pgm_b_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(ppm_b_header), std::end(ppm_b_header), std::begin(header), Image::header_cmp);
}

class Pnm final: public Image
{
public:
    Pnm(const Header & header, std::istream & input);

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

    void read_P1(std::istream & input);
    void read_P2(std::istream & input);
    void read_P3(std::istream & input);
    void read_P4(std::istream & input);
    void read_P5(std::istream & input);
    void read_P6(std::istream & input);
};
#endif // PNM_HPP
