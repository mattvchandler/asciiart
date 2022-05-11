#include "ico.hpp"

#include <sstream>
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
            Png png_img {input, Args{}};
            swap(png_img);
            #else
            throw std::runtime_error{"Could not read PNG encoded ICO / CUR: Not compiled with PNG support"};
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


enum class Ico_type: std::uint16_t {ico = 1, cur = 2};

void ico_write_common(std::ostream & out, const Image & img, bool invert, Ico_type ico_type)
{
    if(img.get_width() > 256 || img.get_height() > 256)
        throw std::runtime_error{"Image dimensions (" + std::to_string(img.get_width()) + "x" + std::to_string(img.get_height()) + ") exceed max CUR/ICO size (256x256)"};

    // generate either png or bmp data
    std::ostringstream image_data;
    #ifdef PNG_FOUND
    if(ico_type == Ico_type::ico && (img.get_width() > 48 || img.get_height() > 48))
    {
        Png::write(image_data, img, invert);
    }
    else
    {
    #endif
        write_bmp_info_header(image_data, img.get_width(), img.get_height(), false, true);
        write_bmp_data(image_data, img, invert);
    #ifdef PNG_FOUND
    }
    #endif
    auto img_data_buf = image_data.str(); // TODO: c++20 adds a view method. use that to prevent this copy

    // ICONDIR struct
    writeb(out, std::uint16_t{0});                                                                        // reserved. must be 0
    writeb(out, static_cast<std::uint16_t>(ico_type));                                                    // ico / cur type
    writeb(out, std::uint16_t{1});                                                                        // number of images in the file

    // ICONDIRENTRY struct
    writeb(out, img.get_width()  >= 256 ? std::uint8_t{0} : static_cast<std::uint8_t>(img.get_width()));  // width
    writeb(out, img.get_height() >= 256 ? std::uint8_t{0} : static_cast<std::uint8_t>(img.get_height())); // height
    writeb(out, std::uint8_t{0});                                                                         // # of palette colors
    writeb(out, std::uint8_t{0});                                                                         // reserved. must be 0
    if(ico_type == Ico_type::ico)
    {
        writeb(out, std::uint16_t{1});  // # of color planes
        writeb(out, std::uint16_t{32}); // bpp
    }
    else // if(ico_type == Ico_type::cur)
    {
        writeb(out, std::uint16_t{0}); // cursor hotspot x coord (UL corner)
        writeb(out, std::uint16_t{0}); // cursor hotspot y coord
    }
    writeb(out, static_cast<std::uint32_t>(std::size(img_data_buf))); // image data size
    writeb(out, std::uint32_t{6 + 16}); //offset of image data. ICONDIR (6 bytes) + 1x ICONDIRENTRY (16 bytes);

    out.write(std::data(img_data_buf), std::size(img_data_buf));
}

void Ico::write_cur(std::ostream & out, const Image & img, bool invert)
{
    ico_write_common(out, img, invert, Ico_type::cur);
}
void Ico::write_ico(std::ostream & out, const Image & img, bool invert)
{
    ico_write_common(out, img, invert, Ico_type::ico);
}
