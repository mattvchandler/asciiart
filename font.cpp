#include "font.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "config.h"

#if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

[[nodiscard]] std::string get_font_path(const std::string & font_name)
{
#if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
    struct Fontconfig
    {
        FcConfig * config {nullptr};
        Fontconfig()
        {
            if(!FcInit())
                throw std::runtime_error{"Error loading fontconfig library"};
            config = FcInitLoadConfigAndFonts();
        }
        ~Fontconfig()
        {
            FcConfigDestroy(config);
            FcFini();
        }
        operator FcConfig *() { return config; }
        operator const FcConfig *() const { return config; }
    };

    Fontconfig fc;

    // get a list of fonts matching the given name
    struct Pattern
    {
        FcPattern * pat{nullptr};
        explicit Pattern(FcPattern * pat): pat{pat} {}
        ~Pattern() { if(pat) FcPatternDestroy(pat); }
        operator FcPattern*() { return pat; }
        operator const FcPattern*() const { return pat; }
    };

    Pattern font_pat {FcNameParse(reinterpret_cast<const FcChar8*>(font_name.c_str()))};
    FcConfigSubstitute(fc, font_pat, FcMatchPattern);
    FcDefaultSubstitute(font_pat);

    struct FontSet
    {
        FcFontSet * set{nullptr};
        explicit FontSet(FcFontSet * set): set{set} {}
        ~FontSet() { if(set) FcFontSetDestroy(set); }
        operator const FcFontSet*() const { return set; }
        operator FcFontSet*() { return set; }
        FcFontSet * operator->() { return set; };
        const FcFontSet * operator->() const { return set; };
        FcPattern* operator[](int i) { return set->fonts[i]; }
        const FcPattern* operator[](int i) const { return set->fonts[i]; }
    };

    FcResult result;
    FontSet fonts {FcFontSort(fc, font_pat, false, NULL, &result)};
    if(result != FcResultMatch)
        throw std::runtime_error{"Error finding font matching: " + font_name};

    for(int i = 0; i < fonts->nfont; ++i)
    {
        // filter out any fonts that aren't monospaced
        int spacing;
        if(FcPatternGetInteger(fonts[i], FC_SPACING, 0, &spacing) != FcResultMatch)
            continue;

        if(spacing != FC_MONO)
            continue;

        FcChar8 * font_path;
        if(FcPatternGetString(fonts[i], FC_FILE, 0, &font_path) != FcResultMatch)
            throw std::runtime_error{"Could not get path to: " + font_name};

        return {reinterpret_cast<const char*>(font_path)};
    }

    throw std::runtime_error{"No fonts found matching: " + font_name};
#else
    // supress unused parameter warnings
    (void)font_name;

    return {};
#endif
}

[[nodiscard]] Char_vals get_char_values(const std::string & font_path, float font_size)
{
#if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
    struct Freetype
    {
        FT_Library lib {nullptr};
        Freetype()
        {
            if(FT_Init_FreeType(&lib) != FT_Err_Ok)
                throw std::runtime_error{"Error loading Freetype library"};
        }
        ~Freetype() { if(lib) FT_Done_FreeType(lib); }
        operator FT_Library() { return lib; }
        operator FT_Library() const { return lib; }
    };

    struct Face
    {
        FT_Face face {nullptr};
        Face(FT_Library ft, const std::string & font_path)
        {
            if(FT_New_Face(ft, font_path.c_str(), 0, &face) != FT_Err_Ok)
                throw std::runtime_error{"Error opening font file: " + font_path};
            if(!face->charmap)
                throw std::runtime_error{"Error font does not contain unicode charmap"};
        }
        ~Face() { if(face) FT_Done_Face(face); }
        operator FT_Face() { return face; }
        operator FT_Face() const { return face; }
        FT_Face operator->() { return face; }
        FT_Face operator->() const { return face; }
    };

    Freetype ft;
    Face face(ft, font_path);

    if(FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(64.0f * font_size), 0, 0) != FT_Err_Ok)
        throw std::runtime_error{"Error setting font size: " + std::to_string(font_size)};

    Char_vals char_vals;
    auto char_width  = FT_MulFix(face->max_advance_width, face->size->metrics.x_scale) / 64;
    auto char_height = FT_MulFix(face->height, face->size->metrics.y_scale) / 64;
    std::vector<std::pair<char, float>> values;

    for(char ch = ' '; ch <= '~'; ++ch)
    {
        if(FT_Load_Char(face, ch, FT_LOAD_RENDER) != FT_Err_Ok)
            throw std::runtime_error{"Error loading char: " + std::string{ch}};

        auto & bmp = face->glyph->bitmap;

        float sum {0.0f};

        for(std::size_t y = 0; y < static_cast<std::size_t>(bmp.rows); ++y)
        {
            for(std::size_t x = 0; x < static_cast<std::size_t>(bmp.width); ++x)
            {
                sum += bmp.buffer[y * bmp.width +x];
            }
        }

        values.emplace_back(ch, sum / (char_width * char_height));
    }

    std::sort(std::begin(values), std::end(values), [](const auto & a, const auto & b) { return a.second < b.second; });

    // change value range to 0-255, and assign a char for each number in that range
    auto min = values.front().second;
    auto max = values.back().second;
    auto range = max - min;

    for(std::size_t i = 0, j = 0; i < std::size(char_vals); ++i)
    {
        if((values[j].second * 255 / range + min) < i && j < std::size(values) - 1)
            ++j;

        char_vals[i] = values[j].first;
    }

    return char_vals;
#else
    // supress unused parameter warnings
    (void)font_path, (void)font_size;
    // if we don't have fontconfig / freetype, use this pregenerated list instead
    return
    {
        ' ', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`',
        '`', '`', '`', '`', '`', '.', '.', '.', '.', '.', '.', '.', '.', '-',
        '\'', ',', ',', ',', ',', ',', ',', ':', ':', ':', ':', ':', ':', ':',
        ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', '"', '"', '~',
        '^', '^', ';', ';', ';', '_', '_', '_', '!', '!', '!', '!', '!', '!',
        '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '*', '*',
        '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '\\',
        '\\', '/', 'r', '(', '(', ')', '|', '|', '+', '>', '<', '=', '?', 'c',
        'c', 'c', 'c', 'c', 'c', 'l', 'l', 'l', 'l', 'l', 'l', 'l', 'l', 'i',
        'i', 'i', 'i', '[', ']', 'v', 's', 'L', '7', 'j', 'z', 'x', 'x', 't',
        'J', '}', '{', 'T', 'Y', '1', 'f', 'C', 'n', 'n', 'n', 'n', 'n', 'u',
        'I', 'I', 'I', 'I', 'I', 'o', 'o', '2', 'F', '3', '3', '3', 'S', 'S',
        'S', 'y', 'y', 'y', '5', 'V', 'e', 'a', 'w', 'w', 'w', 'Z', 'h', 'h',
        '4', 'X', '%', 'k', 'P', 'P', '$', '$', 'G', 'G', 'G', 'G', 'G', 'U',
        'U', 'U', 'E', 'E', '&', 'm', 'b', 'b', 'b', 'b', 'd', '9', 'p', 'q',
        'A', '6', 'O', 'K', '#', '0', 'H', 'H', '8', '8', '8', '8', '8', 'D',
        'D', 'g', 'g', 'R', 'Q', 'Q', 'Q', 'Q', '@', '@', '@', '@', '@', '@',
        '@', '@', '@', '@', '@', '@', '@', '@', '@', 'B', 'B', 'B', 'N', 'W',
        'W', 'M', 'M', 'M'
    };
#endif
}
