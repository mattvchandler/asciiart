#include "font.hpp"

[[nodiscard]] std::string get_font_path(const std::string & font_name)
{
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
        Pattern(FcPattern * pat): pat{pat} {}
        ~Pattern() { if(pat) FcPatternDestroy(pat); }
        operator FcPattern*() { return pat; }
        operator const FcPattern*() const { return pat; }
    };

    Pattern font_pat = FcNameParse(reinterpret_cast<const FcChar8*>(font_name.c_str()));
    FcConfigSubstitute(fc, font_pat, FcMatchPattern);
    FcDefaultSubstitute(font_pat);

    struct FontSet
    {
        FcFontSet * set{nullptr};
        FontSet(FcFontSet * set): set{set} {}
        ~FontSet() { if(set) FcFontSetDestroy(set); }
        operator const FcFontSet*() const { return set; }
        operator FcFontSet*() { return set; }
        FcFontSet * operator->() { return set; };
        const FcFontSet * operator->() const { return set; };
        FcPattern* operator[](int i) { return set->fonts[i]; }
        const FcPattern* operator[](int i) const { return set->fonts[i]; }
    };

    FcResult result;
    FontSet fonts = FcFontSort(fc, font_pat, false, NULL, &result);
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
}

[[nodiscard]] Char_vals get_char_values(const std::string & font_path, float font_size)
{
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
            throw std::runtime_error{"Error loading char: A"};

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
}
