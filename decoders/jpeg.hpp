#ifndef JPEG_HPP
#define JPEG_HPP

#ifdef HAS_JPEG
#include "image.hpp"

#include <istream>
#include <vector>

#include <csetjmp>

#include <jpeglib.h>

class Jpeg final: public Image
{
public:
    Jpeg(const Header & header, std::istream & input);
    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    };

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }

private:

    struct my_jpeg_error: public jpeg_error_mgr
    {
        jmp_buf setjmp_buffer;
        static void exit(j_common_ptr cinfo) noexcept;
    };

    class my_jpeg_source: public jpeg_source_mgr
    {
    public:
        my_jpeg_source(const Header & header, std::istream & input);

    private:

        static boolean my_fill_input_buffer(j_decompress_ptr cinfo) noexcept;
        static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes) noexcept;

        const Header & header_;
        std::istream & input_;
        std::array<JOCTET, 4096> buffer_;
        JOCTET * buffer_p_ { std::data(buffer_) };

        std::size_t header_bytes_read_ {0};
    };

    std::size_t width_{0};
    std::size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;
};
#endif
#endif // JPEG_HPP
