#ifndef BMP_COMMON_HPP
#define BMP_COMMON_HPP

#include <stdexcept>
#include <cstdint>

#include "image.hpp"

struct bmp_data
{
    std::uint32_t pixel_offset {0};
    std::size_t width{0};
    std::size_t height{0};
    bool bottom_to_top {true};
    std::uint16_t bpp {0};
    enum class Compression: std::uint32_t {BI_RGB=0, BI_RLE8=1, BI_RLE4=2, BI_BITFIELDS=3} compression{Compression::BI_RGB};
    std::uint32_t palette_size {0};
    std::uint32_t red_mask {0};
    std::uint32_t green_mask {0};
    std::uint32_t blue_mask {0};
    std::uint32_t alpha_mask {0};

    std::vector<Color> palette;
};

void read_bmp_file_header(std::istream & in, bmp_data & bmp, std::size_t & file_pos);
void read_bmp_info_header(std::istream & in, bmp_data & bmp, std::size_t & file_pos);
void read_bmp_data(std::istream & in, const bmp_data & bmp, std::size_t & file_pos, std::vector<std::vector<Color>> & image_data);

#endif // BMP_COMMON_HPP
