#ifndef JPEG_HPP
#define JPEG_HPP

#ifdef HAS_JPEG
#include <istream>
#include <vector>

#include <csetjmp>

#include <jpeglib.h>
#endif

#include "image.hpp"

inline bool is_jpeg(const Image::Header & header)
{
    const std::array<unsigned char, 4>  jpeg_header1   = {0XFF, 0xD8, 0xFF, 0xDB};
    const std::array<unsigned char, 12> jpeg_header2   = {0XFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01};
    const std::array<unsigned char, 4>  jpeg_header3   = {0XFF, 0xD8, 0xFF, 0xEE};
    const std::array<unsigned char, 4>  jpeg_header4_1 = {0XFF, 0xD8, 0xFF, 0xE1};
    const std::array<unsigned char, 6>  jpeg_header4_2 = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00}; // there are 2 "don't care" bytes between these

    return std::equal(std::begin(jpeg_header1),   std::end(jpeg_header1),   std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpeg_header2),   std::end(jpeg_header2),   std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), Image::header_cmp)
       ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), Image::header_cmp)
       || (std::equal(std::begin(jpeg_header4_1), std::end(jpeg_header4_1), std::begin(header), Image::header_cmp)
           && std::equal(std::begin(jpeg_header4_2), std::end(jpeg_header4_2), std::begin(header) + std::size(jpeg_header4_1) + 2, Image::header_cmp));
}

#ifdef HAS_JPEG
class Jpeg final: public Image
{
public:
    Jpeg(const Header & header, std::istream & input);

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
};
#endif
#endif // JPEG_HPP
