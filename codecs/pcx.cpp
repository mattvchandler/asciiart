#include "pcx.hpp"

#include <limits>
#include <stdexcept>

#include "binio.hpp"

enum class Encoding : std::uint8_t { none = 0, rle = 1 };

Pcx::Pcx(std::istream & input)
{
    const std::array<Color, 16> std_ega_palette
    {{
        {0x00, 0x00, 0x00}, // Black
        {0x00, 0x00, 0xAA}, // Blue
        {0x00, 0xAA, 0x00}, // Green
        {0x00, 0xAA, 0xAA}, // Cyan
        {0xAA, 0x00, 0x00}, // Red
        {0xAA, 0x00, 0xAA}, // Magenta
        {0xAA, 0x55, 0x00}, // Brown
        {0xAA, 0xAA, 0xAA}, // Lt Gray
        {0x55, 0x55, 0x55}, // Dk Gray
        {0x55, 0x55, 0xFF}, // Bright Blue
        {0x55, 0xFF, 0x55}, // Bright Green
        {0x55, 0xFF, 0xFF}, // Bright Cyan
        {0xFF, 0x55, 0x55}, // Bright Red
        {0xFF, 0x55, 0xFF}, // Bright Magenta
        {0xFF, 0xFF, 0x55}, // Bright Yellow
        {0xFF, 0xFF, 0xFF}  // Bright White
    }};

    const Color * palette = std::data(std_ega_palette);
    std::size_t palette_size = std::size(std_ega_palette);

    // TODO: some variations in palettes, bpp, # planes not tested due to lack of available images for testing. No 2 bpp, or RGBi images were found
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        input.ignore(1); // skip magic

        std::uint8_t version;
        readb(input, version);

        Encoding encoding_type;
        readb(input, encoding_type);

        std::uint8_t bpp;
        readb(input, bpp); // bits per pixel plane

        if(bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8)
            throw std::runtime_error{"Invalid bits per pixel plane (" + std::to_string(bpp) + ") for PCX. must be 1,2,4, or 8"};

        std::uint16_t min_x, max_x, min_y, max_y;
        readb(input, min_x);
        readb(input, min_y);
        readb(input, max_x);
        readb(input, max_y);

        set_size(max_x - min_x + 1, max_y - min_y + 1);

        input.ignore(4); // skip DPI

        std::array<Color, 16> ega_palette;
        for(auto && c: ega_palette)
        {
            readb(input, c.r);
            readb(input, c.g);
            readb(input, c.b);
            c.a = 255;
        }

        if(version != 0 || version != 3)
            palette = std::data(ega_palette);

        input.ignore(1); // reserved

        std::uint8_t num_color_planes;
        readb(input, num_color_planes);

        if(num_color_planes != 1 && num_color_planes != 3 && num_color_planes != 4)
            throw std::runtime_error{"Invalid number of color planes (" + std::to_string(num_color_planes) + ") for PCX. must be 1, 3, or 4"};

        std::uint16_t bytes_per_line_per_plane;
        readb(input, bytes_per_line_per_plane);

        enum class Palette  : uint16_t {color = 1, grayscale = 2} palette_type;
        readb(input, palette_type);

        if(palette_type != Palette::color && palette_type != Palette::grayscale)
            throw std::runtime_error{"Unknown PCX palette type: " + std::to_string(static_cast<std::underlying_type_t<Palette>>(palette_type))};

        input.ignore(58); // screen size, and reserved

        enum class Color_type {RGB_24, RGBA_32, RGB_12, RGBA_16, RGB_6, RGBA_8, indexed_256, indexed_16, indexed_4, grayscale_256, grayscale_16, grayscale_4, mono, RGBi} color_type{};

        // TODO: a lot of this is not standard. We're trying to support as much as possible, given that there is no consistent spec for some of these combinations
        // Also, CGA palette handling is not supported
        if(bpp == 8 && num_color_planes == 4)
            color_type = Color_type::RGBA_32;

        else if(bpp == 8 && num_color_planes == 3)
            color_type = Color_type::RGB_24;

        else if(bpp == 8 && num_color_planes == 1 && palette_type == Palette::color)
            color_type = Color_type::indexed_256;

        else if(bpp == 8 && num_color_planes == 1 && palette_type == Palette::grayscale)
            color_type = Color_type::grayscale_256;

        else if(bpp == 4 && num_color_planes == 4)
            color_type = Color_type::RGBA_16;

        else if(bpp == 4 && num_color_planes == 3)
            color_type = Color_type::RGB_12;

        else if(bpp == 4 && num_color_planes == 1 && palette_type == Palette::color)
            color_type = Color_type::indexed_16;

        else if(bpp == 4 && num_color_planes == 1 && palette_type == Palette::grayscale)
            color_type = Color_type::grayscale_16;

        else if(bpp == 2 && num_color_planes == 4)
            color_type = Color_type::RGBA_8;

        else if(bpp == 2 && num_color_planes == 3)
            color_type = Color_type::RGB_6;

        else if(bpp == 2 && num_color_planes == 1 && palette_type == Palette::color)
            color_type = Color_type::indexed_4;

        else if(bpp == 2 && num_color_planes == 1 && palette_type == Palette::grayscale)
            color_type = Color_type::grayscale_4;

        else if(bpp == 1  && num_color_planes == 4)
            color_type = Color_type::RGBi;

        else if(bpp == 1  && num_color_planes == 1)
            color_type = Color_type::mono;

        else
            throw std::runtime_error{"Unsupported PCX image format"};

        std::size_t calculated_bytes_per_line_per_plane = bytes_per_line_per_plane * num_color_planes;
        std::size_t min_needed_bytes_per_line_per_plane = 1 + (width_ * num_color_planes - 1) / (8 / bpp);
        min_needed_bytes_per_line_per_plane += min_needed_bytes_per_line_per_plane % 4 == 0 ? 0 : 4 - min_needed_bytes_per_line_per_plane % 4; // round up to next multiple of 4

        if(calculated_bytes_per_line_per_plane > min_needed_bytes_per_line_per_plane)
            throw std::runtime_error{"PCX bytes per line per plane too large"};

        std::vector<uint8_t> decompressed(calculated_bytes_per_line_per_plane);
        std::vector<std::vector<Color>> decoded(height_);

        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t i = 0; i < std::size(decompressed); ++i)
            {
                std::uint8_t b;
                readb(input, b);

                if(encoding_type == Encoding::none)
                {
                    decompressed[i] = b;
                }
                else
                {
                    // RLE decoding
                    if((b & 0xC0) == 0xC0)
                    {
                        std::uint8_t count = b & 0x3F, value;
                        readb(input, value);

                        if(i + count > std::size(decompressed))
                            throw std::runtime_error{"PCX RLE run length out of bounds"};

                        for(std::size_t j = 0; j < count; ++j)
                            decompressed[i + j] = value;

                        i += count - 1;
                    }
                    else
                    {
                        decompressed[i] = b;
                    }
                }
            }

            decoded[row].resize(width_);

            for(std::size_t i = 0; i < std::size(decompressed); ++i)
            {
                if(bpp == 8)
                {
                    if((1 * i + 0) % (bytes_per_line_per_plane * 1) < width_)
                        decoded[row][(1 * i + 0) % (bytes_per_line_per_plane * 1)][(1 * i + 0) / (bytes_per_line_per_plane * 1)] = decompressed[i];
                }
                else if(bpp == 4)
                {
                    if((2 * i + 0) % (bytes_per_line_per_plane * 2) < width_)
                        decoded[row][(2 * i + 0) % (bytes_per_line_per_plane * 2)][(2 * i + 0) / (bytes_per_line_per_plane * 2)] = (decompressed[i] & 0xF0) >> 4;

                    if((2 * i + 1) % (bytes_per_line_per_plane * 2) < width_)
                        decoded[row][(2 * i + 1) % (bytes_per_line_per_plane * 2)][(2 * i + 1) / (bytes_per_line_per_plane * 2)] = (decompressed[i] & 0x0F);
                }
                else if(bpp == 2)
                {
                    if((4 * i + 0) % (bytes_per_line_per_plane * 4) < width_)
                        decoded[row][(4 * i + 0) % (bytes_per_line_per_plane * 4)][(4 * i + 0) / (bytes_per_line_per_plane * 4)] = (decompressed[i] & 0xC0) >> 6;

                    if((4 * i + 1) % (bytes_per_line_per_plane * 4) < width_)
                        decoded[row][(4 * i + 1) % (bytes_per_line_per_plane * 4)][(4 * i + 1) / (bytes_per_line_per_plane * 4)] = (decompressed[i] & 0x30) >> 4;

                    if((4 * i + 2) % (bytes_per_line_per_plane * 4) < width_)
                        decoded[row][(4 * i + 2) % (bytes_per_line_per_plane * 4)][(4 * i + 2) / (bytes_per_line_per_plane * 4)] = (decompressed[i] & 0x0C) >> 2;

                    if((4 * i + 3) % (bytes_per_line_per_plane * 4) < width_)
                        decoded[row][(4 * i + 3) % (bytes_per_line_per_plane * 4)][(4 * i + 3) / (bytes_per_line_per_plane * 4)] = (decompressed[i] & 0x03);
                }
                else if(bpp == 1)
                {
                    if((8 * i + 0) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 0) % (bytes_per_line_per_plane * 8)][(8 * i + 0) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x80) >> 7;

                    if((8 * i + 1) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 1) % (bytes_per_line_per_plane * 8)][(8 * i + 1) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x40) >> 6;

                    if((8 * i + 2) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 2) % (bytes_per_line_per_plane * 8)][(8 * i + 2) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x20) >> 5;

                    if((8 * i + 3) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 3) % (bytes_per_line_per_plane * 8)][(8 * i + 3) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x10) >> 4;

                    if((8 * i + 4) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 4) % (bytes_per_line_per_plane * 8)][(8 * i + 4) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x08) >> 3;

                    if((8 * i + 5) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 5) % (bytes_per_line_per_plane * 8)][(8 * i + 5) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x04) >> 2;

                    if((8 * i + 6) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 6) % (bytes_per_line_per_plane * 8)][(8 * i + 6) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x02) >> 1;

                    if((8 * i + 7) % (bytes_per_line_per_plane * 8) < width_)
                        decoded[row][(8 * i + 7) % (bytes_per_line_per_plane * 8)][(8 * i + 7) / (bytes_per_line_per_plane * 8)] = (decompressed[i] & 0x01);
                }
            }
        }
        // check for VGA palette
        std::uint8_t vga_indicator {0};

        // suppress eof error
        try { readb(input, vga_indicator); }
        catch(std::ios_base::failure&)
        {
            if(input.bad())
                throw;
        }

        std::array<Color, 256> vga_palette{};
        if(vga_indicator == 0xC)
        {
            palette_size = 0;
            for(auto && c: vga_palette)
            {
                // suppress eof error
                try { readb(input, c.r); }
                catch(std::ios_base::failure&)
                {
                    if(input.bad())
                        throw;
                    else
                        break;
                }

                readb(input, c.g);
                readb(input, c.b);
                c.a = 255;

                ++palette_size;
            }

            palette = std::data(vga_palette);
        }

        //  de-index if needed and store to image data
        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                auto & src = decoded[row][col];
                auto & dest = image_data_[row][col];
                switch(color_type)
                {
                    case Color_type::RGB_24:
                        dest = src;
                        dest.a = 255;
                        break;
                    case Color_type::RGBA_32:
                        dest = src;
                        break;
                    case Color_type::RGB_12:
                        dest.r = src.r << 4;
                        dest.g = src.g << 4;
                        dest.b = src.b << 4;
                        dest.a = 255;
                        break;
                    case Color_type::RGBA_16:
                        dest.r = src.r << 4;
                        dest.g = src.g << 4;
                        dest.b = src.b << 4;
                        dest.a = src.a << 4;
                        break;
                    case Color_type::RGB_6:
                        dest.r = src.r << 6;
                        dest.g = src.g << 6;
                        dest.b = src.b << 6;
                        dest.a = 255;
                        break;
                    case Color_type::RGBA_8:
                        dest.r = src.r << 6;
                        dest.g = src.g << 6;
                        dest.b = src.b << 6;
                        dest.a = src.a << 6;
                        break;
                    case Color_type::indexed_256:
                    case Color_type::indexed_16:
                    case Color_type::indexed_4:
                        if(src.r >= palette_size)
                            throw std::runtime_error{"PCX index out of range: " + std::to_string(src.r) + " with palette size: " + std::to_string(palette_size)};
                        dest = palette[src.r];
                        break;
                    case Color_type::grayscale_256:
                        dest = Color{src.r};
                        break;
                    case Color_type::grayscale_16:
                        dest = Color{static_cast<unsigned char>(src.r << 4)};
                        break;
                    case Color_type::grayscale_4:
                        dest = Color{static_cast<unsigned char>(src.r << 6)};
                        break;
                    case Color_type::mono:
                        dest = src.r ? Color{255} : Color{0};
                        break;
                    case Color_type::RGBi:
                        dest = std_ega_palette [ (src.a << 3) | (src.r << 2) | (src.g << 1) | src.b ];
                        break;
                }
            }
        }
    }
    catch(std::ios_base::failure&)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading PCX: could not read file"};
        else
            throw std::runtime_error{"Error reading PCX: unexpected end of file"};
    }
}

void Pcx::write(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    if(img.get_width() > std::numeric_limits<std::uint16_t>::max() || img.get_height() > std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error{"Image dimensions (" + std::to_string(img.get_width()) + "x" + std::to_string(img.get_height()) + ") exceed max PCX size (" + std::to_string(std::numeric_limits<std::uint16_t>::max()) + "x" + std::to_string(std::numeric_limits<std::uint16_t>::max()) + ")"};

    writeb(out, std::uint8_t{10});                                 // header
    writeb(out, std::uint8_t{5});                                  // version
    writeb(out, Encoding::rle);                                    // encoding
    writeb(out, std::uint8_t{8});                                  // bpp
    writeb(out, std::uint16_t{0});                                 // min x coord
    writeb(out, std::uint16_t{0});                                 // min y coord
    writeb(out, static_cast<std::uint16_t>(img.get_width() - 1));  // width
    writeb(out, static_cast<std::uint16_t>(img.get_height() - 1)); // height
    writeb(out, std::uint16_t{300});                               // x DPI
    writeb(out, std::uint16_t{300});                               // y DPI

    const std::array<char, 48> ega_palette {};
    out.write(std::data(ega_palette), std::size(ega_palette));

    writeb(out, std::uint8_t{0});  // reserved
    writeb(out, std::uint8_t{3});  // num color planes
    writeb(out, static_cast<std::uint16_t>(img.get_width() + (img.get_width() % 4 == 0 ? 0 : 4 - img.get_width() % 4))); // bytes per plane per scanline - rounded up to next multiple of 4
    writeb(out, std::uint16_t{1}); // palette type

    const std::array<char, 58> reserved {};
    out.write(std::data(reserved), std::size(reserved));

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t plane = 0; plane < 3; ++plane)
        {
            for(std::size_t col = 0; col < img.get_width();)
            {
                auto b = img[row][col][plane];

                std::uint8_t rle_count {1};

                // count number of identical consecutive values
                for(std::size_t x = col + 1; x < img.get_width() && img[row][x][plane] == b && rle_count < 63u; ++x, ++rle_count);

                // blend alpha
                b = static_cast<decltype(b)>((b / 255.0f * img[row][col].a / 255.0f + bg / 255.0f * (1.0f - img[row][col].a / 255.0f)) * 255.0f);

                if(invert)
                    b = 255 - b;

                if(rle_count > 1 || (b & 0xC0u) == 0xC0u)
                    writeb(out, static_cast<std::uint8_t>(0xC0u | rle_count));

                writeb(out, b);

                col += rle_count;
            }

            if(img.get_width() % 2 !=0)
                writeb(out, uint8_t{0});
        }
    }
}
