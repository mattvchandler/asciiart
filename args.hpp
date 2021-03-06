#ifndef ARGS_HPP
#define ARGS_HPP

#include <optional>
#include <string>
#include <utility>

#include "config.h"

struct Args
{
    std::string input_filename;  // - for stdin
    std::string output_filename; // - for stdout
    std::string font_name;       // use fontconfig to find, freetype to open
    float       font_size;       // font size requested, in points
    int rows;                    // output rows
    int cols;                    // output cols
    unsigned char bg;            // BG color value
    bool invert;                 // invert colors
    enum class Color {NONE, ANSI4, ANSI8, ANSI24} color;
    enum class Disp_char {HALF_BLOCK, SPACE, ASCII} disp_char;
    enum class Force_file
    {
        detect,                  // detect filetype by header
        pcx,
        tga,
    #ifdef SVG_FOUND
        svg,
    #endif
    #ifdef XPM_FOUND
        xpm,
    #endif
    #ifdef ZLIB_FOUND
        mcmap,
    #endif
        aoc_2019_sif,
    } force_file { Force_file::detect };
    std::optional<std::pair<std::string, std::string>> convert_filename;
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[]);

#endif // ARGS_HPP
