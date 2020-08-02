#ifndef ARGS_HPP
#define ARGS_HPP

#include <filesystem>
#include <optional>
#include <string>

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
    bool force_ascii;
    enum class Force_file
    {
        detect,                  // detect filetype by header
        tga,
    #ifdef SVG_FOUND
        svg,
    #endif
    #ifdef XPM_FOUND
        xpm,
    #endif
        aoc_2019_sif,
    } force_file { Force_file::detect };
    std::optional<std::filesystem::path> convert_filename;
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[]);

#endif // ARGS_HPP
