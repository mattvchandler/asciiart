#include "bmp.hpp"

#include <bitset>

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
    uint32_t red_mask {0};
    uint32_t green_mask {0};
    uint32_t blue_mask {0};
    uint32_t alpha_mask {0};
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
            in.readb(bmp.raw_size); // TODO: needed?
            in.ignore(8);
            in.readb(bmp.palette_size);
            in.ignore(4);

            if(header_size > 40) // V3+
            {
                in.readb(bmp.red_mask);
                in.readb(bmp.green_mask);
                in.readb(bmp.blue_mask);
                in.readb(bmp.alpha_mask);
            }
            break;
        }
        default:
            throw std::runtime_error {"Unsupported BMP header size: " + std::to_string(header_size)};
    }

    // skip to end of header
    auto file_pos = in.tellg();
    in.ignore(14 + header_size - file_pos); // file header is 14 bytes

    if(header_size == 40 && bmp.compression == bmp_data::Compression::BI_BITFIELDS)
    {
        in.readb(bmp.red_mask);
        in.readb(bmp.green_mask);
        in.readb(bmp.blue_mask);
    }

    if(bmp.bpp != 1 && bmp.bpp != 4 && bmp.bpp != 8 && bmp.bpp != 16 && bmp.bpp != 24 && bmp.bpp != 32)
        throw std::runtime_error {"Unsupported bit depth: " + std::to_string(bmp.bpp)};

    if(bmp.palette_size > (1 << bmp.bpp))
        throw std::runtime_error {"Invalid palette size: " + std::to_string(bmp.palette_size)};

    if(bmp.compression != bmp_data::Compression::BI_RGB &&
        bmp.compression != bmp_data::Compression::BI_RLE8 &&
        bmp.compression != bmp_data::Compression::BI_RLE4 &&
        bmp.compression != bmp_data::Compression::BI_BITFIELDS)
    {
        throw std::runtime_error {"Unsupported compression selection: " + std::to_string(static_cast<std::underlying_type_t<bmp_data::Compression>>(bmp.compression))};
    }

    return bmp;
}

Bmp::Bmp(const Header & header, std::istream & input, int bg)
{
    Header_stream in {header, input};
    auto bmp = read_header(in);

    std::cout<<"bpp: "<<bmp.bpp<<'\n';

    width_ = bmp.width;
    height_ = bmp.height;

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

    std::cout<<"masks: "<<std::hex<<bmp.red_mask<<' '<<bmp.green_mask<<' '<<bmp.blue_mask<<' '<<bmp.alpha_mask<<'\n';

    struct color {uint8_t b, g, r, a;};
    std::vector<color> palette;
    if(bmp.bpp < 16)
    {
        palette.resize(bmp.palette_size == 0 ? 1<<bmp.bpp : bmp.palette_size);
        in.read(reinterpret_cast<char*>(std::data(palette)), std::size(palette) * sizeof(color));
    }

    // std::cout<<"palette:\n";
    // for(auto && i :palette)
    //     std::cout<<std::hex<<(int)i.r<<','<<(int)i.g<<','<<(int)i.b<<','<<(int)i.a<<'\n';

    // skip to pixel data
    auto file_pos = in.tellg();
    std::cout<<"offset: "<<file_pos<<' '<<bmp.pixel_offset<<' '<<(bmp.pixel_offset - file_pos)<<'\n';

    if(file_pos > bmp.pixel_offset)
        throw std::runtime_error {"Invalid BMP pixel offset value"};

    in.ignore(bmp.pixel_offset - file_pos);

    if(bmp.compression != bmp_data::Compression::BI_RGB)
    {
        throw std::runtime_error {"compression not implemented yet: " + std::to_string(static_cast<std::underlying_type_t<bmp_data::Compression>>(bmp.compression))};
    }

    std::vector<char> rowbuf((bmp.bpp * width_ + 31) / 32  * 4);
    for(std::size_t row = 0; row < height_; ++row)
    {
        auto im_row = bmp.bottom_to_top ? height_ - row - 1 : row;
        in.read(std::data(rowbuf), std::size(rowbuf));

        if(bmp.compression == bmp_data::Compression::BI_RGB)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                if(bmp.bpp == 1 && col % 8 == 0)
                {
                    std::bitset<8> bits = static_cast<unsigned char>(rowbuf[col / 8]);
                    for(std::size_t i = 0; i < 8 && col + i < width_; ++i)
                    {
                        auto color = palette[bits[7 - i]];
                        image_data_[im_row][col + i] = rgb_to_gray(color.r, color.g, color.b);
                    }
                }
                else if(bmp.bpp == 4 && col % 2 == 0)
                {
                    auto packed = static_cast<unsigned char>(rowbuf[col / 2]);

                    auto color = palette[packed >> 4];
                    image_data_[im_row][col] = rgb_to_gray(color.r, color.g, color.b);

                    color = palette[packed & 0xF];
                    image_data_[im_row][col+ 1] = rgb_to_gray(color.r, color.g, color.b);
                }
                else if(bmp.bpp == 8)
                {
                    auto idx = static_cast<unsigned char>(rowbuf[col]);
                    auto color = palette[idx];
                    image_data_[im_row][col] = rgb_to_gray(color.r, color.g, color.b);
                }
                else if(bmp.bpp == 16)
                {
                    //5.5.5.0.1 format
                    // bbbbbggg ggrrrrrx
                    auto hi = static_cast<unsigned char>(rowbuf[2 * col]);
                    auto lo = static_cast<unsigned char>(rowbuf[2 * col + 1]);
                    unsigned char b = (hi >> 2);
                    unsigned char g = ((hi & 0x07) << 2) | (lo >> 6);
                    unsigned char r = (lo & 0x3E) >> 1;
                    image_data_[im_row][col] = rgb_to_gray(r, g, b);
                }
                else if(bmp.bpp == 24)
                {
                    auto b = static_cast<unsigned char>(rowbuf[3 * col]);
                    auto g = static_cast<unsigned char>(rowbuf[3 * col + 1]);
                    auto r = static_cast<unsigned char>(rowbuf[3 * col + 2]);
                    image_data_[im_row][col] = rgb_to_gray(r, g, b);
                }
                else if(bmp.bpp == 32)
                {
                    auto a = static_cast<unsigned char>(rowbuf[4 * col]);
                    auto b = static_cast<unsigned char>(rowbuf[4 * col + 1]);
                    auto g = static_cast<unsigned char>(rowbuf[4 * col + 2]);
                    auto r = static_cast<unsigned char>(rowbuf[4 * col + 3]);
                    auto val = rgb_to_gray(r, g, b) / 255.0f;
                    auto alpha = a/ 255.0f;
                    image_data_[im_row][col] = static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
                }
            }
        }
        // std::cout<<'\n';
    }
}
