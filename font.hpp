#ifndef FONT_HPP
#define FONT_HPP

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

using Char_vals = std::array<char, 256>;

[[nodiscard]] std::string get_font_path(const std::string & font_name);
[[nodiscard]] Char_vals get_char_values(const std::string & font_path, float font_size);

#endif // FONT_HPP
