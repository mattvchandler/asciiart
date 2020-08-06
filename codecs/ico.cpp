#include "ico.hpp"

#include <stdexcept>

#include <cstdint>

#include "bmp_common.hpp"
#include "png.hpp"
#include "binio.hpp"

struct Ico_header
{
    std::uint8_t width {0};
    std::uint8_t height {0};
    std::uint32_t size {0};
    std::uint32_t offset {0};
};

Ico::Ico(std::istream & input)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        Ico_header ico;

        input.ignore(4); // skip 0 and type

        std::uint16_t num_images;
        readb(input, num_images);

        if(num_images == 0)
            throw std::runtime_error{"Error reading ICO / CUR: 0 images"};

        readb(input, ico.width);
        readb(input, ico.height);
        input.ignore(6); // skip data we can get from the image itself
        readb(input, ico.size);
        readb(input, ico.offset);

        // skip to image data
        input.ignore(ico.offset - 6 - 16); // 6 for ICONDIR, 16 per ICONDIRENTRY

        // determine if image data is BMP or PNG
        Image::Header header;
        input.read(std::data(header), std::size(header));
        for(auto i = std::rbegin(header); i != std::rend(header); ++i)
            input.putback(*i);

        if(is_png(header))
        {
            #ifdef PNG_FOUND
            Png png_img {input};
            swap(png_img);
            #else
            throw std::runtime_error{"Could not read PNG encoded IOC / CUR: Not compiled with PNG support"};
            #endif
            return;
        }

        // assuming BMP format
        width_ = ico.width ? ico.width : 256;
        height_ = ico.height ? ico.width: 256;

        std::size_t file_pos {0};
        bmp_data bmp;
        read_bmp_info_header(input, bmp, file_pos);

        if(bmp.width != width_ || (bmp.height != height_ && bmp.height != 2 * height_))
            throw std::runtime_error{"Error reading ICO / CUR: size mismatch"};

        set_size(width_, height_);

        bool has_and_mask = bmp.height == ico.height * 2;
        if(has_and_mask)
            bmp.height = ico.height;

        // get XOR mask
        read_bmp_data(input, bmp, file_pos, image_data_);

        // get & apply AND mask
        if(has_and_mask && bmp.bpp != 32)
        {
            bmp.bpp = 1;
            bmp.palette = {Color{0, 0, 0, 0xFF}, Color{0, 0, 0, 0x00}};
            auto and_mask = image_data_;

            read_bmp_data(input, bmp, file_pos, and_mask);

            for(std::size_t row = 0; row < height_; ++row)
            {
                for(std::size_t col = 0; col < width_; ++col)
                    image_data_[row][col].a &= and_mask[row][col].a;
            }
        }
    }
    catch(std::ios_base::failure&)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading ICO / CUR: could not read file"};
        else
            throw std::runtime_error{"Error reading ICO / CUR: unexpected end of file"};
    }
}
