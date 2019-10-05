#ifndef GIF_HPP
#define GIF_HPP

#ifdef HAS_GIF
#include <istream>
#include <vector>

#include <gif_lib.h>
#endif

#include "image.hpp"

inline bool is_gif(const Image::Header & header)
{
    const std::array<unsigned char, 6> gif_header1 = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61}; //GIF87a
    const std::array<unsigned char, 6> gif_header2 = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61}; //GIF89a

    return std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), Image::header_cmp);
}

#ifdef HAS_GIF
class Gif final: public Image
{
public:
    Gif(const Header & header, std::istream & input, int bg);

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

    int read_fn(GifByteType * data, int length) noexcept;
    static int read_fn(GifFileType* gif_file, GifByteType * data, int length) noexcept;
};
#endif
#endif // GIF_HPP
