#include "png.hpp"

#include <iostream>
#include <stdexcept>

#include <csetjmp>

#include <png.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto in = static_cast<std::istream *>(png_get_io_ptr(png_ptr));
    if(!in)
        std::longjmp(png_jmpbuf(png_ptr), 1);

    in->read(reinterpret_cast<char *>(data), length);
    if(in->bad())
        std::longjmp(png_jmpbuf(png_ptr), 1);
}
Png::Png(std::istream & input, const Args & args)
{
    handle_extra_args(args);
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }
    auto end_info_ptr = png_create_info_struct(png_ptr);
    if(!end_info_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        throw std::runtime_error{"Error reading with libpng"};
    }

    // set custom read callback (to read from header / c++ istream)
    png_set_read_fn(png_ptr, &input, read_fn);

    // don't care about non-image data
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);

    // get image properties
    png_read_info(png_ptr, info_ptr);

    set_size(png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr));

    auto bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    auto color_type = png_get_color_type(png_ptr, info_ptr);

    // set transformations to convert to 32-bit RGBA
    if(!(color_type & PNG_COLOR_MASK_COLOR))
        png_set_gray_to_rgb(png_ptr);

    if(color_type & PNG_COLOR_MASK_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if(!(color_type & PNG_COLOR_MASK_ALPHA))
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

    if(bit_depth < 8)
        png_set_packing(png_ptr);

    auto number_of_passes = png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    if(png_get_rowbytes(png_ptr, info_ptr) != width_ * 4)
        throw std::runtime_error{"PNG bytes per row incorrect"};

    for(decltype(number_of_passes) pass = 0; pass < number_of_passes; ++pass)
    {
        for(size_t row = 0; row < height_; ++row)
        {
            png_read_row(png_ptr, reinterpret_cast<unsigned char*>(std::data(image_data_[row])), NULL);
        }
    }

    #ifdef EXIF_FOUND
    auto orientation { exif::Orientation::r_0};

    png_bytep exif = nullptr;
    png_uint_32 exif_length = 0;

    png_read_end(png_ptr, end_info_ptr);
    if(png_get_eXIf_1(png_ptr, info_ptr, &exif_length, &exif) != 0 || png_get_eXIf_1(png_ptr, end_info_ptr, &exif_length, &exif) != 0)
    {
        if (exif_length > 1)
        {
            std::vector<unsigned char> exif_buf(exif_length + 6);
            for(auto i = 0; i < 6; ++i)
                exif_buf[i] = "Exif\0\0"[i];
            std::copy(exif, exif + exif_length, std::begin(exif_buf) + 6);

            orientation = exif::get_orientation(std::data(exif_buf), std::size(exif_buf));
        }
    }

    transpose_image(orientation);
    #endif

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
}

void write_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        std::longjmp(png_jmpbuf(png_ptr), 1);

    out->write(reinterpret_cast<char *>(data), length);
    if(out->bad())
        std::longjmp(png_jmpbuf(png_ptr), 1);
}
void flush_fn(png_structp png_ptr)
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        std::longjmp(png_jmpbuf(png_ptr), 1);

    out->flush();
}
void Png::write(std::ostream & out, const Image & img, bool invert)
{
    const Image * img_p = &img;

    std::unique_ptr<Image> img_copy{nullptr};
    if(invert)
    {
        img_copy = std::make_unique<Image>(img.get_width(), img.get_height());
        for(std::size_t row = 0; row < img_copy->get_height(); ++row)
        {
            for(std::size_t col = 0; col < img_copy->get_width(); ++col)
            {
                FColor fcolor {img[row][col]};
                if(invert)
                    fcolor.invert();
                (*img_copy)[row][col] = fcolor;
            }
        }
        img_p = img_copy.get();
    }

    std::vector<const unsigned char *> row_ptrs(img_p->get_height());
    for(std::size_t i = 0; i < img_p->get_height(); ++i)
        row_ptrs[i] = reinterpret_cast<decltype(row_ptrs)::value_type>(std::data((*img_p)[i]));

    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        throw std::runtime_error{"Error writing with libpng"};
    }

    // set custom write callbacks to write to std::ostream
    png_set_write_fn(png_ptr, &out, write_fn, flush_fn);

    png_set_IHDR(png_ptr, info_ptr,
                 img.get_width(), img.get_height(),
                 8, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_rows(png_ptr, info_ptr, const_cast<png_bytepp>(std::data(row_ptrs)));

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

    png_write_end(png_ptr, nullptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
}
