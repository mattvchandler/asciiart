#ifndef ARGS_HPP
#define ARGS_HPP

#include <string>
#include <optional>

struct Args
{
    std::string input_filename;  // - for stdin
    std::string output_filename; // - for stdout
    std::string font_name;       // use fontconfig to find, freetype to open
    float       font_size;       // font size requested, in points
    int rows;                    // output rows
    int cols;                    // output cols
    int bg;                      // BG color value
    bool invert;                 // invert colors
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[]);

#endif // ARGS_HPP