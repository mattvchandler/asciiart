#include "asciiart.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

#include <cmath>
#include <cstring>

#include "color.hpp"

constexpr auto build_color_table()
{
    // Color codes as defined in https://en.wikipedia.org/wiki/ANSI_escape_code
    std::array<Color, 256> table;
    std::array<Color, 16> table_4bit
    {
        Color{  0,   0,   0, 255},
        Color{170,   0,   0, 255},
        Color{  0, 170,   0, 255},
        Color{170,  85,   0, 255},
        Color{  0,   0, 170, 255},
        Color{170,   0, 170, 255},
        Color{  0, 170, 170, 255},
        Color{170, 170, 170, 255},
        Color{ 85,  85,  85, 255},
        Color{255,  85,  85, 255},
        Color{ 85, 255,  85, 255},
        Color{255, 255,  85, 255},
        Color{ 85,  85, 255, 255},
        Color{255,  85, 255, 255},
        Color{ 85, 255, 255, 255},
        Color{255, 255, 255, 255}
    };

    static_assert(std::size(table_4bit) <= std::size(table));

    std::size_t i = 0;
    for(; i < std::size(table_4bit); ++i)
        table[i] = table_4bit[i];

    std::array<unsigned char, 6> levels {0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF};

    for(unsigned char r = 0; r < 6; ++r)
    {
        for(unsigned char g = 0; g < 6; ++g)
        {
            for(unsigned char b = 0; b < 6; ++b)
            {
                table[i++] = Color{levels[r], levels[g], levels[b], 255};
            }
        }
    }

    for(unsigned char y = 0x08; y <= 0xee; y += 0x0A)
        table[i++] = Color {y, y, y, 255};

    return table;
};

constexpr auto color_table = build_color_table();

enum class Color_mode {FG, BG};
class set_color
{
public:
    explicit set_color(const Color & color, Args::Color color_type, Color_mode color_mode)
    {
        if(color_type != Args::Color::ANSI4 && color_type != Args::Color::ANSI8 && color_type != Args::Color::ANSI24)
            throw std::runtime_error{"Unsupported set_color mode"};

        std::ostringstream os;
        os<<"\x1B[";

        if (color_type == Args::Color::ANSI24)
        {
            if (color_mode == Color_mode::FG)
                os << "38";
            else
                os << "48";

            os << ";2;" << static_cast<int>(color.r) << ';' << static_cast<int>(color.g) << ';' << static_cast<int>(color.b);
        }
        else
        {
            if(color_type == Args::Color::ANSI8)
            {
                auto index = std::distance(std::begin(color_table), find_closest_palette_color(std::begin(color_table), std::end(color_table), color));

                if(color_mode == Color_mode::FG)
                    os << "38";
                else
                    os << "48";

                os << ";5;" << index;
            }
            else // ANSI4
            {
                auto index = std::distance(std::begin(color_table), find_closest_palette_color(std::begin(color_table), std::begin(color_table) + 16, color));

                int offset = 40;

                if(color_mode == Color_mode::FG)
                    offset = 30;

                if(index < 8)
                    os <<  index + offset;
                else if(index < 16)
                    os << index + offset + 60 - 8;
                else
                    throw std::runtime_error{"ASNI4 index out of range"};
            }
        }

        os << 'm';
        command_ = os.str();
    }

    friend std::ostream & operator<<(std::ostream &os, const set_color & sc)
    {
        return os << sc.command_;
    }

private:
    std::string command_;
};

std::ostream & clear_color(std::ostream & os)
{
    return os<<"\x1B[0m";
}

void write_ascii(const Image & img, const Char_vals & char_vals, const Args & args)
{
    std::ofstream output_file;
    if(args.output_filename != "-")
        output_file.open(args.output_filename);
    std::ostream & out = args.output_filename == "-" ? std::cout : output_file;

    if(!out)
        throw std::runtime_error{"Could not open output file " + (args.output_filename == "-" ? "" : ("(" + args.output_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

    const auto bg = args.bg / 255.0f;

    auto scaled_img = img.scale(args.cols, (args.rows > 0 ? args.rows : img.get_height() * args.cols / img.get_width() / 2));

    for(std::size_t row = 0; row < scaled_img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < scaled_img.get_width(); ++col)
        {
            FColor color = scaled_img[row][col];
            color.alpha_blend(bg);

            char disp_char = ' ';
            if(args.force_ascii || args.color == Args::Color::NONE)
                disp_char = char_vals[static_cast<unsigned char>(color.to_gray() * 255.0f)];

            if(args.color == Args::Color::NONE)
            {
                out<<disp_char;
            }
            else
            {
                if(args.force_ascii)
                    out<<set_color(color, args.color, Color_mode::FG)<<disp_char;
                else
                    out<<set_color(color, args.color, Color_mode::BG)<<' ';
            }
        }

        if(args.color != Args::Color::NONE)
            out<<clear_color;
        out<<'\n';
    }
}
