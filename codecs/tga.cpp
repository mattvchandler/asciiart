#include "tga.hpp"

#include <stdexcept>

#include "binio.hpp"

struct Tga_data
{
    bool rle_compressed {false};
    enum class Color_type: std::uint8_t { indexed=1, color=2, grayscale=3 } color;
    std::uint16_t width {0};
    std::uint16_t height {0};
    std::uint8_t bpp {0};
    bool bottom_to_top { true};

    std::vector<Color> palette;
};

Color read_pixel(const unsigned char * byte, Tga_data::Color_type color, std::uint8_t bpp, const std::vector<Color> & palette)
{
    if(color == Tga_data::Color_type::color)
    {
        Color c;

        switch(bpp)
        {
        case 15:
        case 16:
            // arrrrrgg gggbbbbb
            if(bpp == 16)
                c.a = ((byte[1] >> 7) & 0x01) * 255;

            c.r = (byte[1] & 0x7C) << 1;
            c.g = ((byte[1] & 0x03) << 6) | ((byte[0] & 0xE0) >> 2);
            c.b = (byte[0] & 0x1F) << 3;

            break;
        case 24:
            c.b = byte[0];
            c.g = byte[1];
            c.r = byte[2];
            break;
        case 32:
            c.b = byte[0];
            c.g = byte[1];
            c.r = byte[2];
            c.a = byte[3];
            break;
        }

        return c;
    }
    else if(color == Tga_data::Color_type::indexed)
    {
        switch(bpp)
        {
        case 8:
            return palette[byte[0]];
            break;
        case 15:
        case 16:
        {
            std::uint16_t idx = static_cast<std::uint16_t>(byte[1]) << 8 | static_cast<std::uint16_t>(byte[0]);
            return palette[idx];
        }
        }
    }
    else if(color == Tga_data::Color_type::grayscale)
    {
        return Color{byte[0]};
    }

    return {};
}

Tga_data read_tga_header(std::istream & in)
{
    Tga_data tga;

    std::uint8_t id_length;
    readb(in, id_length);

    std::uint8_t color_map_type;
    readb(in, color_map_type);
    if(color_map_type > 1)
        throw std::runtime_error {"Unsupported TGA color map type: " + std::to_string((int)color_map_type)};

    std::uint8_t image_type = 0;
    readb(in, image_type);
    if(    image_type != 1 && image_type != 2  && image_type != 3
        && image_type != 9 && image_type != 10 && image_type != 11)
    {
        throw std::runtime_error {"Unsupported TGA image type: " + std::to_string((int)image_type)};
    }

    tga.rle_compressed = image_type & 0x8;
    tga.color = static_cast<Tga_data::Color_type>(image_type & 0x3);

    std::uint16_t color_map_start_idx, color_map_num_entries;
    std::uint8_t color_map_bpp;
    readb(in, color_map_start_idx);
    readb(in, color_map_num_entries);
    readb(in, color_map_bpp);

    if(color_map_bpp != 0 && color_map_bpp != 8 && color_map_bpp != 15 && color_map_bpp != 16 && color_map_bpp != 24 && color_map_bpp != 32)
        throw std::runtime_error{"Unsupported TGA palette color depth: " + std::to_string((int)color_map_bpp)};

    in.ignore(4); // skip origin
    readb(in, tga.width);
    readb(in, tga.height);
    readb(in, tga.bpp);

    std::uint8_t image_descriptor;
    readb(in, image_descriptor);
    tga.bottom_to_top = !((image_descriptor & 0x20) >> 5); // if 0, image is upside down
    auto interleaved = (image_descriptor & 0xC0) >> 6;

    if(interleaved)
        throw std::runtime_error{"TGA interleaving not supported"};

    if(tga.bpp != 8 && tga.bpp != 15 && tga.bpp != 16 && tga.bpp != 24 && tga.bpp != 32)
        throw std::runtime_error{"Unsupported TGA color depth: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color_type::indexed && (tga.bpp == 24 || tga.bpp == 32))
        throw std::runtime_error{"Unsupported TGA color depth in indexed mode: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color_type::color && tga.bpp == 8)
        throw std::runtime_error{"Unsupported TGA color depth in true-color mode: " + std::to_string((int)tga.bpp)};

    if(tga.color == Tga_data::Color_type::grayscale && tga.bpp != 8)
        throw std::runtime_error{"Unsupported TGA color depth in grayscale mode: " + std::to_string((int)tga.bpp)};

    // skip image ID block
    in.ignore(id_length);

    if(color_map_type)
    {
        if(tga.color == Tga_data::Color_type::indexed)
        {
            const std::uint8_t num_bytes = (color_map_bpp + 7) / 8; // ceiling division
            std::vector<char> bytes(num_bytes);

            tga.palette.resize(color_map_num_entries);
            for(std::size_t i = color_map_start_idx; i < color_map_num_entries; ++i)
            {
                in.read(std::data(bytes), num_bytes);
                tga.palette[i] = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), Tga_data::Color_type::color, color_map_bpp, {});
            }
        }
        else // skip the palette
        {
            in.ignore((color_map_num_entries - color_map_start_idx) * color_map_bpp / 8);
        }
    }
    else if(tga.color == Tga_data::Color_type::indexed)
    {
        throw std::runtime_error {"No palette defined for indexed TGA"};
    }

    return tga;
}

void read_uncompressed(std::istream & in, const Tga_data & tga, std::vector<std::vector<Color>> & image_data)
{
    std::vector<char> rowbuf(tga.width * tga.bpp / 8);
    for(std::size_t row = 0; row < tga.height; ++row)
    {
        auto im_row = tga.bottom_to_top ? tga.height - row - 1 : row;
        in.read(std::data(rowbuf), std::size(rowbuf));
        for(std::size_t col = 0; col < tga.width; ++col)
        {
            image_data[im_row][col] = read_pixel(reinterpret_cast<unsigned char *>(std::data(rowbuf)) + (col *((tga.bpp + 7) / 8)), tga.color, tga.bpp, tga.palette); // ceiling division
        }
    }
}

void read_compressed(std::istream & in, const Tga_data & tga, std::vector<std::vector<Color>> & image_data)
{
    std::size_t row{0};
    auto store_val = [&row, col = std::size_t{0}, im_row = (tga.bottom_to_top ? tga.height - row - 1 : row), &tga, &image_data](const Color & color) mutable
    {
        if(row == tga.height)
            throw std::runtime_error{"TGA data out of range"};

        image_data[im_row][col] = color;

        if(++col == tga.width)
        {
            col = 0;
            ++row;
            im_row = tga.bottom_to_top ? tga.height - row - 1 : row;
        }
    };

    const std::uint8_t num_bytes = (tga.bpp + 7) / 8; // ceiling division
    std::vector<char> bytes(num_bytes);

    while(row < tga.height)
    {
        std::uint8_t b;
        readb(in, b);
        auto len = (b & 0x7F) + 1;

        if(b & 0x80) // rle packet
        {
            in.read(std::data(bytes), num_bytes);
            auto val = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), tga.color, tga.bpp, tga.palette);

            for(std::uint8_t i = 0; i < len; ++i)
                store_val(val);
        }
        else // raw packet
        {
            for(std::uint8_t i = 0; i < len; ++i)
            {
                in.read(std::data(bytes), num_bytes);
                auto val = read_pixel(reinterpret_cast<unsigned char *>(std::data(bytes)), tga.color, tga.bpp, tga.palette);
                store_val(val);
            }
        }
    }
}

Tga::Tga(std::istream & input)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        auto tga = read_tga_header(input);

        set_size(tga.width, tga.height);

        if(tga.rle_compressed)
            read_compressed(input, tga, image_data_);
        else
            read_uncompressed(input, tga, image_data_);

    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading TGA: could not read file"};
        else
            throw std::runtime_error{"Error reading TGA: unexpected end of file"};
    }
}

void Tga::write(std::ostream & out, const Image & img, bool invert)
{
    if(img.get_width() > std::numeric_limits<std::uint16_t>::max() || img.get_height() > std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error{"Image dimensions (" + std::to_string(img.get_width()) + "x" + std::to_string(img.get_height()) + ") exceed max TGA size (" + std::to_string(std::numeric_limits<std::uint16_t>::max()) + "x" + std::to_string(std::numeric_limits<std::uint16_t>::max()) + ")"};

    writeb(out, std::uint8_t{0});  // image ID field size (omitted)
    writeb(out, std::uint8_t{0});  // color map type. 0 is none
    writeb(out, std::uint8_t{10}); // image type RLE compressed color

    // color map specification (unused, so all 0s)
    const std::array<std::uint8_t, 5> color_map_spec = {0, 0, 0, 0, 0};
    out.write(reinterpret_cast<const char *>(std::data(color_map_spec)), std::size(color_map_spec));

    writeb(out, std::uint16_t{0});                             // x origin
    writeb(out, std::uint16_t{0});                             // y origin
    writeb(out, static_cast<std::uint16_t>(img.get_width()));  // width
    writeb(out, static_cast<std::uint16_t>(img.get_height())); // height
    writeb(out, std::uint8_t{32});                             // bpp
    writeb(out, std::uint8_t{0});                              // image descriptor (store bottom to top, non-interleaved)

    for(std::size_t row = img.get_height(); row -- > 0;)
    {
        std::vector<Color> non_rle_buf;
        auto write_non_rle = [&non_rle_buf, &out]()
        {
            if(std::empty(non_rle_buf))
                return;

            writeb(out, static_cast<uint8_t>(std::size(non_rle_buf) - 1));

            for(auto && color: non_rle_buf)
            {
                writeb(out, color.b);
                writeb(out, color.g);
                writeb(out, color.r);
                writeb(out, color.a);
            }
            non_rle_buf.clear();
        };

        auto write_rle = [&out](const Color & color, unsigned int count)
        {
            writeb(out, static_cast<uint8_t>((count - 1) | 0x80u));
            writeb(out, color.b);
            writeb(out, color.g);
            writeb(out, color.r);
            writeb(out, color.a);
        };

        for(std::size_t col = 0; col < img.get_width();)
        {
            auto c = img[row][col];

            unsigned int rle_count {1};

            for(std::size_t x = col + 1; x < img.get_width() && img[row][x] == c && rle_count < 128; ++x, ++rle_count); // count number of consecutive pixels with the same color

            if(invert)
            {
                c.r = 255 - c.r;
                c.g = 255 - c.g;
                c.b = 255 - c.b;
            }

            if(rle_count > 2)
            {
                write_non_rle();
                write_rle(c, rle_count);

                col += rle_count;
            }
            else
            {
                non_rle_buf.push_back(c);
                if(std::size(non_rle_buf) == 128)
                    write_non_rle();
                ++col;
            }
        }

        write_non_rle();
    }
}
