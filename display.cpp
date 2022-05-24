#include "display.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <cmath>
#include <cstring>

#include "animate.hpp"
#include "color.hpp"
#include "font.hpp"

#define ESC "\x1B"
#define CSI ESC "["
#define SEP ";"
#define SGR "m"
#define RESET_CHAR CSI "0" SGR
#define FG24 "38;2;"
#define BG24 "48;2;"
#define FG8 "38;5;"
#define BG8 "48;5;"
#define UPPER_HALF_BLOCK "▀";

namespace
{
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

    class set_color
    {
    public:
        explicit set_color(const std::optional<Color> & fg_color, const std::optional<Color> & bg_color, Args::Color color_type)
        {
            if(color_type != Args::Color::NONE && color_type != Args::Color::ANSI4 && color_type != Args::Color::ANSI8 && color_type != Args::Color::ANSI24)
                throw std::runtime_error{"Unsupported set_color mode"};

            enum class Color_mode {none, fg_only, bg_only, both} color_mode;

            if(!fg_color && !bg_color)
                color_mode = Color_mode::none;

            else if(fg_color && !bg_color)
                color_mode = Color_mode::fg_only;

            else if(!fg_color && fg_color)
                color_mode = Color_mode::bg_only;

            else
                color_mode = Color_mode::both;

            std::ostringstream os;

            if(color_type == Args::Color::ANSI24)
            {
                switch(color_mode)
                {
                    case Color_mode::none:
                        break;
                    case Color_mode::fg_only:
                        os << CSI FG24 << static_cast<int>(fg_color->r) << SEP << static_cast<int>(fg_color->g) << SEP << static_cast<int>(fg_color->b) <<SGR;
                        break;
                    case Color_mode::bg_only:
                        os << CSI BG24 << static_cast<int>(bg_color->r) << SEP << static_cast<int>(bg_color->g) << SEP << static_cast<int>(bg_color->b) << SGR;
                        break;
                    case Color_mode::both:
                        os << CSI FG24 << static_cast<int>(fg_color->r) << SEP << static_cast<int>(fg_color->g) << SEP << static_cast<int>(fg_color->b)
                        << SEP BG24 << static_cast<int>(bg_color->r) << SEP << static_cast<int>(bg_color->g) << SEP << static_cast<int>(bg_color->b) << SGR;
                        break;
                }
            }
            else if(color_type == Args::Color::ANSI8)
            {
                switch(color_mode)
                {
                    case Color_mode::none:
                        break;
                    case Color_mode::fg_only:
                        os << CSI FG8 << std::distance(std::begin(color_table), std::find(std::begin(color_table), std::end(color_table), *fg_color)) << SGR;
                        break;
                    case Color_mode::bg_only:
                        os << CSI BG8<< std::distance(std::begin(color_table), std::find(std::begin(color_table), std::end(color_table), *bg_color)) << SGR;
                        break;
                    case Color_mode::both:
                        os << CSI FG8 << std::distance(std::begin(color_table), std::find(std::begin(color_table), std::end(color_table), *fg_color))
                        << SEP BG8 << std::distance(std::begin(color_table), std::find(std::begin(color_table), std::end(color_table), *bg_color)) << SGR;
                        break;
                }
            }
            else if(color_type == Args::Color::ANSI4)
            {
                std::array<int, 2> index {};

                if(fg_color)
                    index[0] = std::distance(std::begin(color_table), std::find(std::begin(color_table), std::begin(color_table) + 16, *fg_color));
                if(bg_color)
                    index[1] = std::distance(std::begin(color_table), std::find(std::begin(color_table), std::begin(color_table) + 16, *bg_color));

                for(auto && i: index)
                {
                    if(i >= 8 && i < 16)
                        i += 60 - 8;
                    else if(i >= 17)
                        throw std::logic_error{"ASNI4 index out of range"};
                }

                index[0] += 30;
                index[1] += 40;

                switch(color_mode)
                {
                    case Color_mode::none:
                        break;
                    case Color_mode::fg_only:
                        os << CSI << index[0] << SGR;
                        break;
                    case Color_mode::bg_only:
                        os << CSI << index[1] << SGR;
                        break;
                    case Color_mode::both:
                        os << CSI << index[0]
                        << SEP << index[1] << SGR;
                        break;
                }
            }

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
        return os << RESET_CHAR;
    }
}

void display_image(const Image & img, const Args & args)
{
    if(args.animate)
    {
        auto animator = Animate{args};
        do
        {
            for(auto f = 0u; f < img.num_frames(); ++f)
            {
                animator.set_frame_delay(args.animation_frame_delay > 0.0f ? std::chrono::duration<float>(args.animation_frame_delay) : img.get_frame_delay(f));
                animator.display(img.get_frame(f));
                if(!animator)
                    break;
            }
        } while(animator && args.loop_animation);
    }
    else
    {
        std::ofstream output_file;
        if(args.output_filename != "-")
            output_file.open(args.output_filename);
        std::ostream & out = args.output_filename == "-" ? std::cout : output_file;

        if(!out)
            throw std::runtime_error{"Could not open output file " + (args.output_filename == "-" ? "" : ("(" + args.output_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

        if(args.frame_no)
            print_image(img.get_frame(*args.frame_no), args, out);
        else
            print_image(img.get_image(args.image_no.value_or(0u)), args, out);
    }
}

void print_image(const Image & img, const Args & args, std::ostream & out)
{
    if(img.get_width() == 0 || img.get_height() == 0)
        return;

    Char_vals char_vals;
    if(args.disp_char == Args::Disp_char::ASCII)
    {
        auto font_path = get_font_path(args.font_name);
        char_vals = get_char_values(font_path, args.font_size);
    }

    const auto bg = args.bg / 255.0f;
    auto disp_height = args.rows > 0 ? args.rows : img.get_height() * args.cols / img.get_width() / 2;
    if(args.disp_char == Args::Disp_char::HALF_BLOCK)
        disp_height *= 2;

    auto scaled_img = img.scale(args.cols, disp_height);

    for(std::size_t row = 0; row < scaled_img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < scaled_img.get_width(); ++col)
        {
            FColor disp_c {scaled_img[row][col]};

            if(args.invert)
                disp_c.invert();

            disp_c.alpha_blend(bg);

            scaled_img[row][col] = disp_c;
        }
    }

    if(args.color == Args::Color::ANSI8)
        scaled_img.dither(std::begin(color_table), std::end(color_table));
    else if(args.color == Args::Color::ANSI4)
        scaled_img.dither(std::begin(color_table), std::begin(color_table) + 16);

    for(std::size_t row = 0; row < (args.disp_char == Args::Disp_char::HALF_BLOCK ? scaled_img.get_height() / 2 : scaled_img.get_height()); ++row)
    {
        for(std::size_t col = 0; col < scaled_img.get_width(); ++col)
        {
            switch(args.disp_char)
            {
                case Args::Disp_char::HALF_BLOCK:
                    out<<set_color(scaled_img[row * 2][col], scaled_img[row * 2 + 1][col], args.color) << UPPER_HALF_BLOCK;
                    break;

                case Args::Disp_char::SPACE:
                    out<<set_color({}, scaled_img[row][col], args.color) << " ";
                    break;

                case Args::Disp_char::ASCII:
                {
                    auto color = scaled_img[row][col];
                    auto disp_char = char_vals[static_cast<unsigned char>(FColor{color}.to_gray() * 255.0f)];
                    out<<set_color(color, {}, args.color) << disp_char;
                    break;
                }
            }
        }

        if(args.color != Args::Color::NONE)
            out<<clear_color;
        out<<'\n';
    }
}
