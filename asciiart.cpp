#include "asciiart.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

#include <cmath>
#include <cstring>

struct FColor
{
    float r{0.0f}, g{0.0f}, b{0.0f}, a{1.0f};
    constexpr FColor(){}
    constexpr FColor(float r, float g, float b, float a = 1.0f):
        r{r},
        g{g},
        b{b},
        a{a}
    {}
    constexpr FColor(const Color & c):
        r{c.r / 255.0f},
        g{c.g / 255.0f},
        b{c.b / 255.0f},
        a{c.a / 255.0f}
    {}

    constexpr operator Color() const
    {
        return {
            static_cast<unsigned char>(r * 255.0f),
            static_cast<unsigned char>(g * 255.0f),
            static_cast<unsigned char>(b * 255.0f),
            static_cast<unsigned char>(a * 255.0f)
        };
    }

    constexpr bool operator<(const FColor & other) const
    {
        if(r < other.r)
            return true;
        else if(g < other.g)
            return true;
        else if(b < other.b)
            return true;
        else if(a < other.a)
            return true;
        else
            return false;
    }

    void alpha_blend(float bg)
    {
        r = r * a + bg * (1.0f - a);
        g = g * a + bg * (1.0f - a);
        b = b * a + bg * (1.0f - a);
        a = 1.0f;
    }
    float to_gray() const
    {
        // formulas from https://www.w3.org/TR/WCAG20/
        std::array<float, 3> luminance_color = { r, g, b };

        for(auto && c: luminance_color)
            c = (c <= 0.03928f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);

        return 0.2126f * luminance_color[0] + 0.7152f * luminance_color[1] + 0.0722f * luminance_color[2];
    }
    void invert()
    {
        r = 1.0f - r;
        g = 1.0f - g;
        b = 1.0f - b;
    }
};

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

float color_dist(const FColor & a, const FColor & b)
{
    auto key = std::pair{a, b};
    static std::map<decltype(key), float> lru_cache;

    if(auto lru = lru_cache.find(key); lru != std::end(lru_cache))
        return lru->second;

    auto dist = std::sqrt((a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b));
    lru_cache.emplace(key, dist);

    return dist;
}

template<Args::Color color_type>
int find_closest_table_color(const Color & color)
{
    static_assert(color_type == Args::Color::ANSI4 || color_type == Args::Color::ANSI8, "Only ANSI4 and ANSI8 colors supported");

    static std::map<Color, std::size_t> lru_cache;

    if(auto lru = lru_cache.find(color); lru != std::end(lru_cache))
        return lru->second;

    auto end = std::end(color_table);
    if constexpr(color_type == Args::Color::ANSI4)
        end = std::begin(color_table) + 16;

    auto min = std::min_element(std::begin(color_table), end, [color = FColor{color}](const Color & a, const Color & b) { return color_dist(a, color) < color_dist(b, color); });
    auto dist = std::distance(std::begin(color_table), min);

    lru_cache.emplace(color, dist);
    return dist;
}

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
                auto index = find_closest_table_color<Args::Color::ANSI8>(color);

                if(color_mode == Color_mode::FG)
                    os << "38";
                else
                    os << "48";

                os << ";5;" << index;
            }
            else // ANSI4
            {
                auto index = find_closest_table_color<Args::Color::ANSI4>(color);

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

    const auto px_col = static_cast<float>(img.get_width()) / args.cols;
    const auto px_row = args.rows > 0 ? static_cast<float>(img.get_height()) / args.rows : px_col * 2.0f;

    const auto bg = args.bg / 255.0f;

    for(float row = 0.0f; row < static_cast<float>(img.get_height()); row += px_row)
    {
        for(float col = 0.0f; col < static_cast<float>(img.get_width()); col += px_col)
        {
            float r_sum = 0.0f;
            float g_sum = 0.0f;
            float b_sum = 0.0f;
            float a_sum = 0.0f;

            float cell_count {0.0f};

            for(float y = row; y < row + px_row && y < img.get_height(); y += 1.0f)
            {
                for(float x = col; x < col + px_col && x < img.get_width(); x += 1.0f)
                {
                    auto x_ind = static_cast<std::size_t>(x);
                    auto y_ind = static_cast<std::size_t>(y);
                    if(x_ind >= img.get_width() || y_ind >= img.get_height())
                        throw std::runtime_error{"Output coords out of range"};

                    auto pix = img[y_ind][x_ind];

                    r_sum += static_cast<float>(pix.r) * static_cast<float>(pix.r);
                    g_sum += static_cast<float>(pix.g) * static_cast<float>(pix.g);
                    b_sum += static_cast<float>(pix.b) * static_cast<float>(pix.b);
                    a_sum += static_cast<float>(pix.a) * static_cast<float>(pix.a);

                    cell_count += 1.0f;
                }
            }

            FColor color {
                std::sqrt(r_sum / cell_count) / 255.0f,
                std::sqrt(g_sum / cell_count) / 255.0f,
                std::sqrt(b_sum / cell_count) / 255.0f,
                std::sqrt(a_sum / cell_count) / 255.0f
            };

            color.alpha_blend(bg);
            if(args.invert)
                color.invert();

            char disp_char = ' ';
            if(args.force_ascii || args.color == Args::Color::NONE)
                disp_char =char_vals[static_cast<unsigned char>(color.to_gray() * 255.0f)];

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
