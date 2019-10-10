#ifndef ARGS_HPP
#define ARGS_HPP

#include <optional>
#include <string>

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
    enum class Force_file
    {
        detect,                  // detect filetype by header
    #ifdef HAS_XPM
        xpm,
    #endif
    } force_file { Force_file::detect };
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[]);

#endif // ARGS_HPP
