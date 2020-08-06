#include "bmp_common.hpp"

#include <bitset>
#include <stdexcept>

#include "binio.hpp"

void read_bmp_file_header(std::istream & in, bmp_data & bmp, std::size_t & file_pos)
{
    in.ignore(10);
    readb(in, bmp.pixel_offset);
    file_pos += 14;
}

void read_bmp_info_header(std::istream & in, bmp_data & bmp, std::size_t & file_pos)
{
    std::uint32_t header_size {0};
    readb(in, header_size);

    // file_pos += 4;
    file_pos = 18;

    switch(header_size)
    {
        case 12: // BITMAPCOREHEADER
        {
            std::int16_t width, height;
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
            std::int32_t width, height;
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

            std::underlying_type_t<bmp_data::Compression> compression;
            readb(in, compression);
            bmp.compression = static_cast<bmp_data::Compression>(compression);

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

    if(bmp.palette_size > (std::uint64_t{1} << bmp.bpp))
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
        bmp.palette.resize(bmp.palette_size == 0 ? std::uint64_t{1}<<bmp.bpp : bmp.palette_size);
        in.read(reinterpret_cast<char*>(std::data(bmp.palette)), std::size(bmp.palette) * sizeof(Color));

        // convert BGR to RGB
        for(auto && i: bmp.palette)
        {
            std::swap(i.r, i.b);
            i.a = 0xFF;
        }

        file_pos += std::size(bmp.palette) * sizeof(Color);
    }

    // skip to pixel data (if we know the offset)
    if(bmp.pixel_offset != 0)
    {
        if(file_pos > bmp.pixel_offset)
            throw std::runtime_error {"Invalid BMP pixel offset value"};

        in.ignore(bmp.pixel_offset - file_pos);
        file_pos = bmp.pixel_offset;
    }
}

void read_uncompressed(std::istream & in, const bmp_data & bmp, std::vector<std::vector<Color>> & image_data)
{
    std::vector<unsigned char> rowbuf((bmp.bpp * bmp.width + 31) / 32  * 4); // ceiling division
    for(std::size_t row = 0; row < bmp.height; ++row)
    {
        auto im_row = bmp.bottom_to_top ? bmp.height - row - 1 : row;
        in.read(reinterpret_cast<char *>(std::data(rowbuf)), std::size(rowbuf));

        for(std::size_t col = 0; col < bmp.width; ++col)
        {
            if(bmp.bpp == 1 && col % 8 == 0)
            {
                std::bitset<8> bits = rowbuf[col / 8];
                for(std::size_t i = 0; i < 8 && col + i < bmp.width; ++i)
                {
                    image_data[im_row][col + i] = bmp.palette[bits[7 - i]];
                }
            }
            else if(bmp.bpp == 4 && col % 2 == 0)
            {
                auto packed = rowbuf[col / 2];

                image_data[im_row][col] = bmp.palette[packed >> 4];

                image_data[im_row][col+ 1] = bmp.palette[packed & 0xF];
            }
            else if(bmp.bpp == 8)
            {
                auto idx = rowbuf[col];
                image_data[im_row][col] = bmp.palette[idx];
            }
            else if(bmp.bpp == 16)
            {
                auto lo = rowbuf[2 * col];
                auto hi = rowbuf[2 * col + 1];
                Color color;

                if(bmp.compression == bmp_data::Compression::BI_RGB)
                {
                    //5.5.5.0.1 format
                    // bbbbbggg ggrrrrrx
                    color.b = hi & 0xF8;
                    color.g = ((hi & 0x07) << 5) | ((lo & 0xC0) >> 3);
                    color.r = (lo & 0x3E) << 2;
                }
                else // BI_BITFIELDS
                {
                    std::uint16_t rshift = 0, rmask = bmp.red_mask;
                    std::uint16_t gshift = 0, gmask = bmp.green_mask;
                    std::uint16_t bshift = 0, bmask = bmp.blue_mask;
                    std::uint16_t ashift = 0, amask = bmp.alpha_mask;

                    for(; rmask != 0 && (rmask & 0x1) == 0; ++rshift) { rmask >>= 1; }
                    for(; gmask != 0 && (gmask & 0x1) == 0; ++gshift) { gmask >>= 1; }
                    for(; bmask != 0 && (bmask & 0x1) == 0; ++bshift) { bmask >>= 1; }
                    for(; amask != 0 && (amask & 0x1) == 0; ++ashift) { amask >>= 1; }

                    std::uint16_t packed = (static_cast<std::uint16_t>(hi) << 8) | static_cast<std::uint16_t>(lo);

                    color.r = (packed & bmp.red_mask)   >> rshift;
                    color.g = (packed & bmp.green_mask) >> gshift;
                    color.b = (packed & bmp.blue_mask)  >> bshift;
                    color.a = (packed & bmp.alpha_mask) >> ashift;

                    if(bmp.alpha_mask == 0)
                        color.a = 0xFF;
                }

                image_data[im_row][col] = color;
            }
            else if(bmp.bpp == 24)
            {
                image_data[im_row][col] = Color{
                    rowbuf[3 * col + 2],
                    rowbuf[3 * col + 1],
                    rowbuf[3 * col],
                    0xFF};
            }
            else if(bmp.bpp == 32)
            {
                auto b1 = rowbuf[4 * col];
                auto b2 = rowbuf[4 * col + 1];
                auto b3 = rowbuf[4 * col + 2];
                auto b4 = rowbuf[4 * col + 3];
                Color color;

                if(bmp.compression == bmp_data::Compression::BI_RGB)
                {
                    color.b = b1; color.g = b2; color.r = b3; color.a = b4;
                }
                else // BI_BITFIELDS
                {
                    std::uint32_t rshift = 0, rmask = bmp.red_mask;
                    std::uint32_t gshift = 0, gmask = bmp.green_mask;
                    std::uint32_t bshift = 0, bmask = bmp.blue_mask;
                    std::uint32_t ashift = 0, amask = bmp.alpha_mask;

                    for(; rmask != 0 && (rmask & 0x1) == 0; ++rshift) { rmask >>= 1; }
                    for(; gmask != 0 && (gmask & 0x1) == 0; ++gshift) { gmask >>= 1; }
                    for(; bmask != 0 && (bmask & 0x1) == 0; ++bshift) { bmask >>= 1; }
                    for(; amask != 0 && (amask & 0x1) == 0; ++ashift) { amask >>= 1; }

                    std::uint32_t packed = (static_cast<std::uint32_t>(b4) << 24) | (static_cast<std::uint32_t>(b3) << 16) | (static_cast<std::uint32_t>(b2) << 8) | static_cast<std::uint32_t>(b1);

                    color.r = (packed & bmp.red_mask)   >> rshift;
                    color.g = (packed & bmp.green_mask) >> gshift;
                    color.b = (packed & bmp.blue_mask)  >> bshift;
                    color.a = (packed & bmp.alpha_mask) >> ashift;

                    if(bmp.alpha_mask == 0)
                        color.a = 0xFF;
                }

                image_data[im_row][col] = color;
            }
        }
    }
}

void read_rle(std::istream & in, const bmp_data & bmp, std::vector<std::vector<Color>> & image_data, std::size_t & file_pos)
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
                    Color color;
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
                        image_data[im_row][col++] = color;
                    }
                }
                else if(bmp.bpp == 8)
                {
                    for(auto i = 0; i < escape; ++i)
                    {
                        auto color = bmp.palette[in.get()];
                        ++file_pos;
                        check_bounds();
                        image_data[im_row][col++] = color;
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
                    image_data[im_row][col++] = color;
                }
            }
            else if(bmp.bpp == 8)
            {
                for(auto i = 0; i < count; ++i)
                {
                    auto color = bmp.palette[idx];
                    check_bounds();
                    image_data[im_row][col++] = color;
                }
            }
        }
    }
}

void read_bmp_data(std::istream & in, const bmp_data & bmp, std::size_t & file_pos, std::vector<std::vector<Color>> & image_data)
{
    if(bmp.compression == bmp_data::Compression::BI_RGB || bmp.compression == bmp_data::Compression::BI_BITFIELDS)
        read_uncompressed(in, bmp, image_data);
    else if(bmp.compression == bmp_data::Compression::BI_RLE8 || bmp.compression == bmp_data::Compression::BI_RLE4)
        read_rle(in, bmp, image_data, file_pos);
}
