#include "png.hpp"

#include <iostream>

Png::Png(const Header & header, std::istream & input, int bg):
    header_{header},
    input_{input}
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
    png_set_read_fn(png_ptr, this, read_fn);

    // don't care about non-image data
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);

    // get image properties
    png_read_info(png_ptr, info_ptr);
    height_ = png_get_image_height(png_ptr, info_ptr);
    width_ = png_get_image_width(png_ptr, info_ptr);
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

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

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
                auto val   = row_buffer[col * 2]     / 255.0f;
                auto alpha = row_buffer[col * 2 + 1] / 255.0f;

                image_data_[row][col] = static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
            }
        }
    }

    std::cout<<(int)image_data_[0][0]<<'\n';

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}

void Png::read_fn(png_bytep data, png_size_t length) noexcept
{
    std::size_t png_ind = 0;
    while(header_bytes_read_ < std::size(header_) && png_ind < length)
        data[png_ind++] = header_[header_bytes_read_++];

    input_.read(reinterpret_cast<char *>(data) + png_ind, length - png_ind);
    if(input_.bad())
    {
        std::cerr<<"FATAL ERROR: Could not read PNG image\n";
        std::exit(EXIT_FAILURE);
    }
}

void Png::read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    Png * png = static_cast<Png *>(png_get_io_ptr(png_ptr));
    if(!png)
    {
        std::cerr<<"FATAL ERROR: Could not get PNG struct pointer\n";
        std::exit(EXIT_FAILURE);
    }

    png->read_fn(data, length);
}
