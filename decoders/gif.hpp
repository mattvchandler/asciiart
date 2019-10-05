#ifndef GIF_HPP
#define GIF_HPP

#ifdef HAS_GIF
#include "image.hpp"

#include <istream>
#include <vector>

#include <gif_lib.h>

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
    const Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};

    int read_fn(GifByteType * data, int length) noexcept;
    static int read_fn(GifFileType* gif_file, GifByteType * data, int length) noexcept;

    std::size_t width_{0};
    std::size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;
};
#endif
#endif // GIF_HPP
