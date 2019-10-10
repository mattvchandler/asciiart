#include "tga.hpp"

#include <cstdint>

struct Tga_data
{
    bool rle_compressed {false};
    enum class Color: uint8_t { indexed=1, color=2, grayscale=3 } color;
    uint16_t width {0};
    uint16_t height {0};
    uint8_t bpp {0};
    bool bottom_to_top { true};

    std::vector<unsigned char> palette;
};

unsigned char read_pixel(const unsigned char * byte, Tga_data::Color color, uint8_t bpp, std::vector<unsigned char> palette, unsigned char bg)
{
    if(color == Tga_data::Color::color)
    {
        unsigned char r{0}, g{0}, b{0}, a{0xFF};

        switch(bpp)
        {
        case 15:
        case 16:
            // arrrrrgg gggbbbbb
            if(bpp == 16)
                a = ((byte[1] >> 7) & 0x01) * 255;

            r = (byte[1] & 0x7C) << 1;
            g = ((byte[1] & 0x03) << 6) | ((byte[0] & 0xE0) >> 2);
            b = (byte[0] & 0x1F) << 3;

            break;
        case 24:
            b = byte[0];
            g = byte[1];
            r = byte[2];
            break;
        case 32:
            b = byte[0];
            g = byte[1];
            r = byte[2];
            a = byte[3];
            break;
        }

        auto val = rgb_to_gray(r, g, b) / 255.0f;
        auto alpha = a / 255.0f;
        return static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
    }
    else if(color == Tga_data::Color::indexed)
    {
        switch(bpp)
        {
        case 8:
            return palette[byte[0]];
            break;
        case 15:
        case 16:
        {
            uint16_t idx = static_cast<uint16_t>(byte[1]) << 8 | static_cast<uint16_t>(byte[0]);
            return palette[idx];
            break;
        }
        }
    }
    else if(color == Tga_data::Color::grayscale)
    {
        return byte[0];
    }
    return 0;
}

Tga_data read_tga_header(Header_stream & in, unsigned char bg)
{
    Tga_data tga;

    uint8_t id_length;
    in.readb(id_length);

    uint8_t color_map_type;
    in.readb(color_map_type);
    if(color_map_type > 1)
        throw std::runtime_error {"Unsupported TGA color map type: " + std::to_string((int)color_map_type)};

    uint8_t image_type = 0;
    in.readb(image_type);
    if(    image_type != 1 && image_type != 2  && image_type != 3
        && image_type != 9 && image_type != 10 && image_type != 11)
    {
        throw std::runtime_error {"Unsupported TGA image type: " + std::to_string((int)image_type)};
    }

    tga.rle_compressed = image_type & 0x8;
    tga.color = static_cast<Tga_data::Color>(image_type & 0x3);

    uint16_t color_map_start_idx, color_map_num_entries;
    uint8_t color_map_bpp;
    in.readb(color_map_start_idx);
    in.readb(color_map_num_entries);
    in.readb(color_map_bpp);

    if(color_map_bpp != 0 && color_map_bpp != 8 && color_map_bpp != 15 && color_map_bpp != 16 && color_map_bpp != 24 && color_map_bpp != 32)
        throw std::runtime_error{"Unsupported TGA palette color depth: " + std::to_string((int)color_map_bpp)};

    in.ignore(4); // skip origin
    in.readb(tga.width);
    in.readb(tga.height);
    in.readb(tga.bpp);

    uint8_t image_descriptor;
    in.readb(image_descriptor);
    tga.bottom_to_top = !((image_descriptor & 0x20) >> 5); // if 0, image is upside down
    auto interleaved = (image_descriptor & 0xC0) >> 6;

    if(interleaved)
        throw std::runtime_error{"TGA interleaving not supported"};

    if(tga.bpp != 8 && tga.bpp != 15 && tga.bpp != 16 && tga.bpp != 24 && tga.bpp != 32)
        throw std::runtime_error{"Unsupported TGA color depth: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color::indexed && (tga.bpp == 24 || tga.bpp == 32))
        throw std::runtime_error{"Unsupported TGA color depth in indexed mode: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color::color && tga.bpp == 8)
        throw std::runtime_error{"Unsupported TGA color depth in true-color mode: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color::grayscale && tga.bpp != 8)
        throw std::runtime_error{"Unsupported TGA color depth in grayscale mode: " + std::to_string((int)tga.bpp)};

    // skip image ID block
    in.ignore(id_length);

    if(color_map_type)
    {
        if(tga.color == Tga_data::Color::indexed)
        {
            const uint8_t num_bytes = (color_map_bpp + 7) / 8; // ceiling division
            std::vector<char> bytes(num_bytes);

            tga.palette.resize(color_map_num_entries);
            for(std::size_t i = color_map_start_idx; i < color_map_num_entries; ++i)
            {
                in.read(std::data(bytes), num_bytes);
                tga.palette[i] = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), Tga_data::Color::color, color_map_bpp, {}, bg);
            }
        }
        else // skip the palette
        {
            in.ignore((color_map_num_entries - color_map_start_idx) * color_map_bpp / 8);
        }
    }
    else if(tga.color == Tga_data::Color::indexed)
    {
        throw std::runtime_error {"No palette defined for indexed TGA"};
    }

    return tga;
}

void read_uncompressed(Header_stream & in, const Tga_data & tga, std::vector<std::vector<unsigned char>> & image_data, unsigned char bg)
{
    std::vector<char> rowbuf(tga.width * tga.bpp / 8);
    for(std::size_t row = 0; row < tga.height; ++row)
    {
        auto im_row = tga.bottom_to_top ? tga.height - row - 1 : row;
        in.read(std::data(rowbuf), std::size(rowbuf));
        for(std::size_t col = 0; col < tga.width; ++col)
        {
            image_data[im_row][col] = read_pixel(reinterpret_cast<unsigned char *>(std::data(rowbuf)) + (col *((tga.bpp + 7) / 8)), tga.color, tga.bpp, tga.palette, bg); // ceiling division
        }
    }
}

void read_compressed(Header_stream & in, const Tga_data & tga, std::vector<std::vector<unsigned char>> & image_data, unsigned char bg)
{
    std::size_t row{0};
    auto store_val = [&row, col = std::size_t{0}, im_row = (tga.bottom_to_top ? tga.height - row - 1 : row), &tga, &image_data](unsigned char val) mutable
    {
        if(row == tga.height)
            throw std::runtime_error{"TGA data out of range"};

        image_data[im_row][col] = val;

        if(++col == tga.width)
        {
            col = 0;
            ++row;
            im_row = tga.bottom_to_top ? tga.height - row - 1 : row;
        }
    };

    const uint8_t num_bytes = (tga.bpp + 7) / 8; // ceiling division
    std::vector<char> bytes(num_bytes);

    while(row < tga.height)
    {
        uint8_t b;
        in.readb(b);
        auto len = (b & 0x7F) + 1;

        if(b & 0x80) // rle packet
        {
            in.read(std::data(bytes), num_bytes);
            auto val = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), tga.color, tga.bpp, tga.palette, bg);

            for(uint8_t i = 0; i < len; ++i)
                store_val(val);
        }
        else // raw packet
        {
            for(uint8_t i = 0; i < len; ++i)
            {
                in.read(std::data(bytes), num_bytes);
                auto val = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), tga.color, tga.bpp, tga.palette, bg);
                store_val(val);
            }
        }
    }
}

Tga::Tga(const Header & header, std::istream & input, unsigned char bg)
{
    Header_stream in {header, input};
    in.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        auto tga = read_tga_header(in, bg);

        width_ = tga.width;
        height_ = tga.height;

        image_data_.resize(height_);
        for(auto && row: image_data_)
            row.resize(width_);

        if(tga.rle_compressed)
            read_compressed(in, tga, image_data_, bg);
        else
            read_uncompressed(in, tga, image_data_, bg);

    }
    catch(std::ios_base::failure & e)
    {
        if(in.bad())
            throw std::runtime_error{"Error reading TGA: could not read file"};
        else
            throw std::runtime_error{"Error reading TGA: unexpected end of file"};
    }
}
