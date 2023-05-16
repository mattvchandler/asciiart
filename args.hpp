#ifndef ARGS_HPP
#define ARGS_HPP

#include <optional>
#include <string>
#include <utility>
#include <vector>

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
    bool display;                // display the image
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
        pkmn_gen1,
        pkmn_gen2,
    #endif
        aoc_2019_sif,
    } force_file { Force_file::detect };
    std::optional<std::pair<std::string, std::string>> convert_filename;
    std::optional<unsigned int> image_no;
    std::optional<unsigned int> frame_no;
    bool get_image_count;
    bool get_frame_count;
    bool animate;
    bool loop_animation;
    float animation_frame_delay;
    std::vector<std::string> extra_args;
    std::string help_text;
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[]);

#endif // ARGS_HPP
