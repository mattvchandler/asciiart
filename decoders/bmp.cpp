#include "bmp.hpp"

#include <algorithm>

#include <cstdint>

#include <iostream> // TODO: deleteme

struct bmp_data
{
    uint32_t pixel_offset {0};
    std::size_t width{0};
    std::size_t height{0};
    bool bottom_to_top {true};
    uint16_t bpp {0};
    enum class Compression: uint32_t {BI_RGB=0, BI_RLE8=1, BI_RLE4=2, BI_BITFIELDS=3} compression{Compression::BI_RGB};
    uint32_t raw_size {0};
    uint32_t palette_size {0};
};

bmp_data read_header(Header_stream & in)
{
    bmp_data bmp;

    in.ignore(10);
    in.readb(bmp.pixel_offset);

    uint32_t header_size {0};
    in.readb(header_size);

    switch(header_size)
    {
        case 12: // BITMAPCOREHEADER
        {
            int16_t width, height;
            in.readb(width);
            if(width < 0)
                width = -width;
            bmp.width = width;

            in.readb(height);
            if(height < 0)
            {
                height = -height;
                bmp.bottom_to_top = false;
            }
            bmp.height = height;
            in.ignore(2);

            in.readb(bmp.bpp);

            break;
        }
        case 40: // BITMAPINFOHEADER
        case 56: // BITMAPV3INFOHEADER
        case 108: // BITMAPV4HEADER
        case 124: // BITMAPV5HEADER
        {
            int32_t width, height;
            in.readb(width);
            if(width < 0)
                width = -width;
            bmp.width = width;

            in.readb(height);
            if(height < 0)
            {
                height = -height;
                bmp.bottom_to_top = false;
            }
            bmp.height = height;

            in.ignore(2);
            in.readb(bmp.bpp);
            in.readb(bmp.compression);
            in.readb(bmp.raw_size);
            in.ignore(8);
            in.readb(bmp.palette_size);
            in.ignore(4);

            if(header_size > 40) // V3+
            {
                throw std::runtime_error {"BMP header not implemented yet"};
            }

            break;
        }
        default:
            throw std::runtime_error {"Unsupported BMP header size: " + std::to_string(header_size)};
    }

    if(bmp.bpp != 1 && bmp.bpp != 4 && bmp.bpp != 8 && bmp.bpp != 16 && bmp.bpp != 24 && bmp.bpp != 32)
        throw std::runtime_error {"Unsupported bit depth: " + std::to_string(bmp.bpp)};

    return bmp;
}

Bmp::Bmp(const Header & header, std::istream & input, int bg)
{
    Header_stream in {header, input};
    auto bmp = read_header(in);

    width_ = bmp.width;
    height_ = bmp.height;

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

    if(bmp.bpp < 16)
    {
        // TODO: read palette
        throw std::runtime_error {"Color table not implemented yet"};
    }

    // skip to pixel data
    auto file_pos = in.tellg();

    std::cout<<long(file_pos)<<' '<<bmp.pixel_offset<<' '<<bmp.pixel_offset - long(file_pos)<<'\n';

    if(file_pos > bmp.pixel_offset)
        throw std::runtime_error {"Invalid BMP pixel offset value"};

    in.ignore(bmp.pixel_offset - file_pos);

    if(bmp.compression != bmp_data::Compression::BI_RGB)
    {
        throw std::runtime_error {"compression not implemented yet"};
    }

    std::cout<<bmp.bpp<<'\n';
    if(bmp.bpp != 24)
        throw std::runtime_error {"Only 24BPP implemented so far"};

    std::vector<char> rowbuf((bmp.bpp * width_ + 31) / 32  * 4);
    std::cout<<std::size(rowbuf)<<'\n';
    for(std::size_t row = 0; row < height_; ++row)
    {
        auto im_row = bmp.bottom_to_top ? height_ - row - 1 : row;
        in.read(std::data(rowbuf), std::size(rowbuf));
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto b = static_cast<unsigned char>(rowbuf[3 * col]);
            auto g = static_cast<unsigned char>(rowbuf[3 * col + 1]);
            auto r = static_cast<unsigned char>(rowbuf[3 * col + 2]);
            image_data_[im_row][col] = rgb_to_gray(r, g, b);
        }
    }
}
