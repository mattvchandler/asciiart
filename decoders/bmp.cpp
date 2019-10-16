#include "bmp.hpp"

#include <bitset>

#include <cstdint>

struct bmp_data
{
    uint32_t pixel_offset {0};
    std::size_t width{0};
    std::size_t height{0};
    bool bottom_to_top {true};
    uint16_t bpp {0};
    enum class Compression: uint32_t {BI_RGB=0, BI_RLE8=1, BI_RLE4=2, BI_BITFIELDS=3} compression{Compression::BI_RGB};
    uint32_t palette_size {0};
    uint32_t red_mask {0};
    uint32_t green_mask {0};
    uint32_t blue_mask {0};
    uint32_t alpha_mask {0};

    struct color {uint8_t b, g, r, a;};
    std::vector<color> palette;
};

bmp_data read_bmp_header(std::istream & in, std::size_t & file_pos)
{
    bmp_data bmp;

    in.ignore(10);
    readb(in, bmp.pixel_offset);

    uint32_t header_size {0};
    readb(in, header_size);

    file_pos += 18;

    switch(header_size)
    {
        case 12: // BITMAPCOREHEADER
        {
            int16_t width, height;
            readb(in, width);
            if(width < 0)
                width = -width;
            bmp.width = width;

            readb(in, height);
            if(height < 0)
            {
                height = -height;
                bmp.bottom_to_top = false;
            }
            bmp.height = height;
            in.ignore(2);

            readb(in, bmp.bpp);

            file_pos += 8;

            break;
        }
        case 40: // BITMAPINFOHEADER
        case 56: // BITMAPV3INFOHEADER
        case 108: // BITMAPV4HEADER
        case 124: // BITMAPV5HEADER
        {
            int32_t width, height;
            readb(in, width);
            if(width < 0)
                width = -width;
            bmp.width = width;

            readb(in, height);
            if(height < 0)
            {
                height = -height;
                bmp.bottom_to_top = false;
            }
            bmp.height = height;

            in.ignore(2);
            readb(in, bmp.bpp);
            readb(in, bmp.compression);
            in.ignore(12);
            readb(in, bmp.palette_size);
            in.ignore(4);

            file_pos += 36;

            if(header_size > 40) // V3+
            {
                readb(in, bmp.red_mask);
                readb(in, bmp.green_mask);
                readb(in, bmp.blue_mask);
                readb(in, bmp.alpha_mask);

                file_pos += 16;
            }
            break;
        }
        default:
            throw std::runtime_error {"Unsupported BMP header size: " + std::to_string(header_size)};
    }

    // skip to end of header
    in.ignore(14 + header_size - file_pos); // file header is 14 bytes
    file_pos = 14 + header_size;

    if(header_size == 40 && bmp.compression == bmp_data::Compression::BI_BITFIELDS)
    {
        readb(in, bmp.red_mask);
        readb(in, bmp.green_mask);
        readb(in, bmp.blue_mask);

        file_pos += 12;
    }

    if(bmp.bpp != 1 && bmp.bpp != 4 && bmp.bpp != 8 && bmp.bpp != 16 && bmp.bpp != 24 && bmp.bpp != 32)
        throw std::runtime_error {"Unsupported bit depth: " + std::to_string(bmp.bpp)};

    if(bmp.palette_size > (1u << bmp.bpp))
        throw std::runtime_error {"Invalid palette size: " + std::to_string(bmp.palette_size)};

    if(bmp.compression != bmp_data::Compression::BI_RGB &&
        bmp.compression != bmp_data::Compression::BI_RLE8 &&
        bmp.compression != bmp_data::Compression::BI_RLE4 &&
        bmp.compression != bmp_data::Compression::BI_BITFIELDS)
    {
        throw std::runtime_error {"Unsupported compression selection: " + std::to_string(static_cast<std::underlying_type_t<bmp_data::Compression>>(bmp.compression))};
    }

    if(bmp.compression == bmp_data::Compression::BI_BITFIELDS && bmp.bpp != 16 && bmp.bpp != 32)
        throw std::runtime_error {"BI_BITFIELDS not supported for bit depth: " + std::to_string(bmp.bpp)};

    if(bmp.compression == bmp_data::Compression::BI_RLE8 && bmp.bpp != 8)
        throw std::runtime_error {"BI_RLE8 not supported for bit depth: " + std::to_string(bmp.bpp)};

    if(bmp.compression == bmp_data::Compression::BI_RLE4 && bmp.bpp != 4)
        throw std::runtime_error {"BI_RLE4 not supported for bit depth: " + std::to_string(bmp.bpp)};

    if(bmp.bpp < 16)
    {
        bmp.palette.resize(bmp.palette_size == 0 ? 1<<bmp.bpp : bmp.palette_size);
        in.read(reinterpret_cast<char*>(std::data(bmp.palette)), std::size(bmp.palette) * sizeof(bmp_data::color));

        file_pos += std::size(bmp.palette) * sizeof(bmp_data::color);
    }

    // skip to pixel data
    if(file_pos > bmp.pixel_offset)
        throw std::runtime_error {"Invalid BMP pixel offset value"};

    in.ignore(bmp.pixel_offset - file_pos);
    file_pos = bmp.pixel_offset;

    return bmp;
}

void read_uncompressed(std::istream & in, bmp_data & bmp, unsigned char bg, std::vector<std::vector<unsigned char>> & image_data)
{
    std::vector<char> rowbuf((bmp.bpp * bmp.width + 31) / 32  * 4); // ceiling division
    for(std::size_t row = 0; row < bmp.height; ++row)
    {
        auto im_row = bmp.bottom_to_top ? bmp.height - row - 1 : row;
        in.read(std::data(rowbuf), std::size(rowbuf));

        for(std::size_t col = 0; col < bmp.width; ++col)
        {
            if(bmp.bpp == 1 && col % 8 == 0)
            {
                std::bitset<8> bits = static_cast<unsigned char>(rowbuf[col / 8]);
                for(std::size_t i = 0; i < 8 && col + i < bmp.width; ++i)
                {
                    auto color = bmp.palette[bits[7 - i]];
                    image_data[im_row][col + i] = rgb_to_gray(color.r, color.g, color.b);
                }
            }
            else if(bmp.bpp == 4 && col % 2 == 0)
            {
                auto packed = static_cast<unsigned char>(rowbuf[col / 2]);

                auto color = bmp.palette[packed >> 4];
                image_data[im_row][col] = rgb_to_gray(color.r, color.g, color.b);

                color = bmp.palette[packed & 0xF];
                image_data[im_row][col+ 1] = rgb_to_gray(color.r, color.g, color.b);
            }
            else if(bmp.bpp == 8)
            {
                auto idx = static_cast<unsigned char>(rowbuf[col]);
                auto color = bmp.palette[idx];
                image_data[im_row][col] = rgb_to_gray(color.r, color.g, color.b);
            }
            else if(bmp.bpp == 16)
            {
                auto lo = static_cast<unsigned char>(rowbuf[2 * col]);
                auto hi = static_cast<unsigned char>(rowbuf[2 * col + 1]);
                unsigned char r{0}, g{0}, b{0}, a{0xFF};

                if(bmp.compression == bmp_data::Compression::BI_RGB)
                {
                    //5.5.5.0.1 format
                    // bbbbbggg ggrrrrrx
                    b = hi & 0xF8;
                    g = ((hi & 0x07) << 5) | ((lo & 0xC0) >> 3);
                    r = (lo & 0x3E) << 2;
                }
                else // BI_BITFIELDS
                {
                    uint16_t rshift = 0, rmask = bmp.red_mask;
                    uint16_t gshift = 0, gmask = bmp.green_mask;
                    uint16_t bshift = 0, bmask = bmp.blue_mask;
                    uint16_t ashift = 0, amask = bmp.alpha_mask;

                    for(; rmask != 0 && (rmask & 0x1) == 0; ++rshift) { rmask >>= 1; }
                    for(; gmask != 0 && (gmask & 0x1) == 0; ++gshift) { gmask >>= 1; }
                    for(; bmask != 0 && (bmask & 0x1) == 0; ++bshift) { bmask >>= 1; }
                    for(; amask != 0 && (amask & 0x1) == 0; ++ashift) { amask >>= 1; }

                    uint16_t packed = (static_cast<uint16_t>(hi) << 8) | static_cast<uint16_t>(lo);

                    r = static_cast<unsigned char>((packed & bmp.red_mask)   >> rshift);
                    g = static_cast<unsigned char>((packed & bmp.green_mask) >> gshift);
                    b = static_cast<unsigned char>((packed & bmp.blue_mask)  >> bshift);
                    a = static_cast<unsigned char>((packed & bmp.alpha_mask) >> ashift);

                    if(bmp.alpha_mask == 0)
                        a = 0xFF;
                }

                image_data[im_row][col] = rgba_to_gray(r, g, b, a, bg);
            }
            else if(bmp.bpp == 24)
            {
                auto b = static_cast<unsigned char>(rowbuf[3 * col]);
                auto g = static_cast<unsigned char>(rowbuf[3 * col + 1]);
                auto r = static_cast<unsigned char>(rowbuf[3 * col + 2]);
                image_data[im_row][col] = rgb_to_gray(r, g, b);
            }
            else if(bmp.bpp == 32)
            {
                auto b1 = static_cast<unsigned char>(rowbuf[4 * col]);
                auto b2 = static_cast<unsigned char>(rowbuf[4 * col + 1]);
                auto b3 = static_cast<unsigned char>(rowbuf[4 * col + 2]);
                auto b4 = static_cast<unsigned char>(rowbuf[4 * col + 3]);
                unsigned char r{0}, g{0}, b{0}, a{0xFF};

                if(bmp.compression == bmp_data::Compression::BI_RGB)
                {
                    b = b1; g = b2; r = b3; a = b4;
                }
                else // BI_BITFIELDS
                {
                    uint32_t rshift = 0, rmask = bmp.red_mask;
                    uint32_t gshift = 0, gmask = bmp.green_mask;
                    uint32_t bshift = 0, bmask = bmp.blue_mask;
                    uint32_t ashift = 0, amask = bmp.alpha_mask;

                    for(; rmask != 0 && (rmask & 0x1) == 0; ++rshift) { rmask >>= 1; }
                    for(; gmask != 0 && (gmask & 0x1) == 0; ++gshift) { gmask >>= 1; }
                    for(; bmask != 0 && (bmask & 0x1) == 0; ++bshift) { bmask >>= 1; }
                    for(; amask != 0 && (amask & 0x1) == 0; ++ashift) { amask >>= 1; }

                    uint32_t packed = (static_cast<uint32_t>(b4) << 24) | (static_cast<uint32_t>(b3) << 16) | (static_cast<uint32_t>(b2) << 8) | static_cast<uint32_t>(b1);

                    r = static_cast<unsigned char>((packed & bmp.red_mask)   >> rshift);
                    g = static_cast<unsigned char>((packed & bmp.green_mask) >> gshift);
                    b = static_cast<unsigned char>((packed & bmp.blue_mask)  >> bshift);
                    a = static_cast<unsigned char>((packed & bmp.alpha_mask) >> ashift);

                    if(bmp.alpha_mask == 0)
                        a = 0xFF;
                }

                image_data[im_row][col] = rgba_to_gray(r, g, b, a, bg);
            }
        }
    }
}

void read_rle(std::istream & in, bmp_data & bmp, std::vector<std::vector<unsigned char>> & image_data, std::size_t & file_pos)
{
    std::size_t row = 0, col = 0;
    auto im_row = bmp.bottom_to_top ? bmp.height - row - 1 : row;

    auto check_bounds = [&row, &col, &bmp]()
    {
        if(row >= bmp.height || col >= bmp.width)
            throw std::runtime_error {"BMP data out of range"};
    };

    while(true)
    {
        unsigned char count = in.get();
        ++file_pos;

        if(count == 0)
        {
            unsigned char escape = in.get();
            ++file_pos;

            if(escape == 0) // end of line
            {
                col = 0;
                ++row;
                im_row = bmp.bottom_to_top ? bmp.height - row - 1 : row;
            }
            else if(escape == 1) // end of bitmap
            {
                break;
            }
            else if(escape == 2) // delta
            {
                unsigned char horiz = in.get();
                unsigned char vert = in.get();
                file_pos += 2;

                col += horiz;
                row += vert;
                im_row = bmp.bottom_to_top ? bmp.height - row - 1 : row;
            }
            else // absolute mode
            {
                if(bmp.bpp == 4)
                {
                    unsigned char idx {0};
                    bmp_data::color color;
                    for(auto i = 0; i < escape; ++i)
                    {
                        if(i % 2 == 0)
                        {
                            idx = in.get();
                            ++file_pos;
                            color = bmp.palette[idx >> 4];
                        }
                        else
                        {
                            color = bmp.palette[idx & 0xF];
                        }

                        check_bounds();
                        image_data[im_row][col++] = rgb_to_gray(color.r, color.g, color.b);
                    }
                }
                else if(bmp.bpp == 8)
                {
                    for(auto i = 0; i < escape; ++i)
                    {
                        auto color = bmp.palette[in.get()];
                        ++file_pos;
                        check_bounds();
                        image_data[im_row][col++] = rgb_to_gray(color.r, color.g, color.b);
                    }
                }
                // align to word boundary
                if(file_pos % 2 != 0)
                {
                    in.ignore(1);
                    ++file_pos;
                }
            }
        }
        else
        {
            unsigned char idx = in.get();
            ++file_pos;
            if(bmp.bpp == 4)
            {
                for(auto i = 0; i < count; ++i)
                {
                    auto color = bmp.palette[(i % 2 == 0) ? (idx >> 4) : (idx & 0x4)];
                    check_bounds();
                    image_data[im_row][col++] = rgb_to_gray(color.r, color.g, color.b);
                }
            }
            else if(bmp.bpp == 8)
            {
                for(auto i = 0; i < count; ++i)
                {
                    auto color = bmp.palette[idx];
                    check_bounds();
                    image_data[im_row][col++] = rgb_to_gray(color.r, color.g, color.b);
                }
            }
        }
    }
}

Bmp::Bmp(std::istream & input, unsigned char bg)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        std::size_t file_pos {0}; // need to keep track of where we are in the file because in.tellg() fails on at least some pipe inputs
        auto bmp = read_bmp_header(input, file_pos);
        set_size(bmp.width, bmp.height);

        if(bmp.compression == bmp_data::Compression::BI_RGB || bmp.compression == bmp_data::Compression::BI_BITFIELDS)
            read_uncompressed(input, bmp, bg, image_data_);
        else if(bmp.compression == bmp_data::Compression::BI_RLE8 || bmp.compression == bmp_data::Compression::BI_RLE4)
            read_rle(input, bmp, image_data_, file_pos);
    }
    catch(std::ios_base::failure&)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading BMP: could not read file"};
        else
            throw std::runtime_error{"Error reading BMP: unexpected end of file"};
    }
}
