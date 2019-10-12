#include "png.hpp"

#include <iostream>

#include <png.h>

struct Png_reader
{
    Png_reader(const Image::Header & header, std::istream & input):
        header_{header}, input_{input}
    {}

    const Image::Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};
};

void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto png = static_cast<Png_reader *>(png_get_io_ptr(png_ptr));
    if(!png)
    {
        std::cerr<<"FATAL ERROR: Could not get PNG struct pointer\n";
        std::exit(EXIT_FAILURE);
    }

    std::size_t png_ind = 0;
    while(png->header_bytes_read_ < std::size(png->header_) && png_ind < length)
        data[png_ind++] = png->header_[png->header_bytes_read_++];

    png->input_.read(reinterpret_cast<char *>(data) + png_ind, length - png_ind);
    if(png->input_.bad())
    {
        std::cerr<<"FATAL ERROR: Could not read PNG image\n";
        std::exit(EXIT_FAILURE);
    }

}
Png::Png(const Header & header, std::istream & input, unsigned char bg)
{
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        throw std::runtime_error{"Error reading with libpng"};
    }

    // set custom read callback (to read from header / c++ istream)
    Png_reader reader{header, input};
    png_set_read_fn(png_ptr, &reader, read_fn);

    // don't care about non-image data
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);

    // get image properties
    png_read_info(png_ptr, info_ptr);

    set_size(png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr));

    auto bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    auto color_type = png_get_color_type(png_ptr, info_ptr);

    // set transformations to convert to 8-bit grayscale w/ alpha
    if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGBA)
        png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);

    if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

    if(bit_depth < 8)
        png_set_packing(png_ptr);

    auto number_of_passes = png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    if(png_get_rowbytes(png_ptr, info_ptr) != width_ * 2)
        throw std::runtime_error{"PNG bytes per row incorrect"};

    // buffer for Gray + Alpha pixels
    std::vector<unsigned char> row_buffer(width_ * 2);

    for(decltype(number_of_passes) pass = 0; pass < number_of_passes; ++pass)
    {
        for(size_t row = 0; row < height_; ++row)
        {
            png_read_row(png_ptr, std::data(row_buffer), NULL);
            for(std::size_t col = 0; col < width_; ++col)
            {
                // alpha blending w/ background
                image_data_[row][col] = ga_blend(row_buffer[col * 2], row_buffer[col * 2 + 1], bg);
            }
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}
