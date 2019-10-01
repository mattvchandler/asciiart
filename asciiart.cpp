// Convert an image input to ascii art
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include <csetjmp>

#include <cxxopts.hpp>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define HAS_PNG
#ifdef HAS_PNG
#include <png.h>
#endif

struct Args
{
    std::string input_filename;  // - for stdin
    std::string output_filename; // - for stdout
    std::string font_name;       // use fontconfig to find, freetype to open
    float       font_size;       // font size requested, in points
    int rows;                    // output rows
    int cols;                    // output cols
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[])
{
    cxxopts::Options options{argv[0], "Dump SQL or tables to CSV"};
    try
    {
        options.add_options()
            ("h,help",   "Show this message and quit")
            ("input",    "",                                                                              cxxopts::value<std::string>()->default_value("-"),          "")
            ("f,font",   "Font name to render. Uses fontconfig to find",                                  cxxopts::value<std::string>()->default_value("monospace"),  "FONT_PATTERN")
            ("s,size",   "Font size, in points",                                                          cxxopts::value<float>()->default_value("12.0"),             "")
            ("r,rows",   "# of outout rows. Enter a negative value to preserve aspect ratio with --cols", cxxopts::value<int>()->default_value("-1"),                 "ROWS")
            ("c,cols",   "# of outout cols",                                                              cxxopts::value<int>()->default_value("80"),                 "COLS")
            ("o,output", "Output text file path. will output to stdout if '-'",                           cxxopts::value<std::string>()->default_value("-"),          "OUTPUT_FILE");

        options.parse_positional("input");
        options.positional_help("INPUT_IMAGE");

        auto args = options.parse(argc, argv);

        if(args.count("help"))
        {
            std::cerr<<options.help()<<'\n';
            return {};
        }

        if(args["rows"].as<int>() == 0)
        {
            std::cerr<<options.help()<<'\n'
                     <<"Value for --rows cannot be 0\n";
            return {};
        }
        if(args["cols"].as<int>() <= 0)
        {
            std::cerr<<options.help()<<'\n'
                     <<"Value for --cols must be positive\n";
            return {};
        }

        return Args{
            args["input"].as<std::string>(),
            args["output"].as<std::string>(),
            args["font"].as<std::string>(),
            args["size"].as<float>(),
            args["rows"].as<int>(),
            args["cols"].as<int>()
        };
    }
    catch(const cxxopts::OptionException & e)
    {
        std::cerr<<options.help()<<'\n'<<e.what()<<'\n';
        return {};
    }
}

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

using Char_vals = std::array<char, 256>;

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

class Image
{
public:
    virtual ~Image() = default;
    virtual unsigned char get_pix(std::size_t row, std::size_t col) const = 0;
    virtual size_t get_width() const = 0;
    virtual size_t get_height() const = 0;
};

#ifdef HAS_PNG
class Png final: public Image
{
public:
    Png(const std::array<char, 12> & header, std::istream & input):
        header_{header},
        input_{input}
    {
        auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if(!png_ptr)
            throw std::runtime_error{"Error initializing libpng"};

        auto info_ptr = png_create_info_struct(png_ptr);
        if(!info_ptr)
        {
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            throw std::runtime_error{"Error initializing libpng info"};
        }

        if(setjmp(png_jmpbuf(png_ptr)))
        {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            throw std::runtime_error{"Error reading with libpng"};
        }

        // set custom read callback (to read from header / c++ istream)
        png_set_read_fn(png_ptr, this, read_fn);

        // don't care about non-image data
        png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);

        // get image properties
        png_read_info(png_ptr, info_ptr);
        height_ = png_get_image_height(png_ptr, info_ptr);
        width_ = png_get_image_width(png_ptr, info_ptr);
        auto bit_depth = png_get_bit_depth(png_ptr, info_ptr);
        auto color_type = png_get_color_type(png_ptr, info_ptr);

        // set transformations to convert to 8-bit grayscale w/ alpha
        if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGBA)
            png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);

        if(color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_strip_alpha(png_ptr);

        if(bit_depth == 16)
            png_set_strip_16(png_ptr);

        if(bit_depth < 8)
            png_set_packing(png_ptr);

        auto number_of_passes = png_set_interlace_handling(png_ptr);

        png_read_update_info(png_ptr, info_ptr);

        if(png_get_rowbytes(png_ptr, info_ptr) != width_)
            throw std::runtime_error{"PNG bytes per row incorrect"};

        image_data_.resize(height_);
        for(auto && row: image_data_)
            row.resize(width_);

        for(decltype(number_of_passes) pass = 0; pass < number_of_passes; ++pass)
        {
            for(size_t row = 0; row < height_; ++row)
            {
                png_read_row(png_ptr, std::data(image_data_[row]), NULL);
            }
        }

        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    }

    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    }

    size_t get_width() const override { return width_; }
    size_t get_height() const override { return height_; }
private:
    std::array<char, 12>  header_;
    std::istream & input_;

    std::size_t bytes_read_ {0};

    size_t width_{0};
    size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;

    void read_fn(png_bytep data, png_size_t length) noexcept
    {
        std::size_t png_ind = 0;
        while(bytes_read_ < std::size(header_) && png_ind < length)
            data[png_ind++] = header_[bytes_read_++];

        input_.read(reinterpret_cast<char *>(data) + png_ind, length - png_ind);
        if(input_.fail())
        {
            std::cerr<<"FATAL ERROR: Could not read PNG image\n";
            std::exit(EXIT_FAILURE);
        }
    }

    static void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
    {
        Png * png = static_cast<Png *>(png_get_io_ptr(png_ptr));
        if(!png)
        {
            std::cerr<<"FATAL ERROR: Could not get Png struct pointer\n";
            std::exit(EXIT_FAILURE);
        }

        png->read_fn(data, length);
    }
};
#endif

[[nodiscard]] std::unique_ptr<Image> get_image_data(std::string & input_filename)
{
    std::ifstream input_file;
    if(input_filename != "-")
        input_file.open(input_filename, std::ios_base::in | std::ios_base::binary);
    std::istream & input = (input_filename == "-") ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file: " + std::string{std::strerror(errno)}};

    std::array<char, 12> header;

    input.read(std::data(header), std::size(header));
    if(!input)
        throw std::runtime_error{"Could not read input file: " + std::string{std::strerror(errno)}};

    auto header_cmp = [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); };

    const std::array<unsigned char, 8>  png_header     = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const std::array<unsigned char, 4>  jpeg_header1   = {0XFF, 0xD8, 0xFF, 0xDB};
    const std::array<unsigned char, 12> jpeg_header2   = {0XFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01};
    const std::array<unsigned char, 4>  jpeg_header3   = {0XFF, 0xD8, 0xFF, 0xEE};
    const std::array<unsigned char, 4>  jpeg_header4_1 = {0XFF, 0xD8, 0xFF, 0xE1};
    const std::array<unsigned char, 6>  jpeg_header4_2 = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00}; // there are 2 "don't care" bytes between these
    const std::array<unsigned char, 6>  gif_header1    = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};
    const std::array<unsigned char, 6>  gif_header2    = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};

    if(std::equal(std::begin(png_header), std::end(png_header), std::begin(header), header_cmp))
    {
    #ifdef HAS_PNG
        return std::make_unique<Png>(header, input);
    #else
        throw std::runtime_error{"Not compiled with PNG support"};
    #endif
    }
    else if(std::equal(std::begin(jpeg_header1),   std::end(jpeg_header1),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header2),   std::end(jpeg_header2),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
        || (std::equal(std::begin(jpeg_header4_1), std::end(jpeg_header4_1), std::begin(header), header_cmp)
            && std::equal(std::begin(jpeg_header4_2), std::end(jpeg_header4_2), std::begin(header) + std::size(jpeg_header4_1) + 2, header_cmp)))
    {
    #ifdef HAS_JPEG
        // return std::make_unique<Jpeg>(header, input);
        throw std::runtime_error{"JPEG not yet supported\n"};
    #else
        throw std::runtime_error{"Not compiled with JPEG support"};
    #endif
    }
    else if(std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), header_cmp)
         || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), header_cmp))
    {
    #ifdef HAS_GIF
        // return std::make_unique<Gif>(header, input);
        throw std::runtime_error{"GIF not yet supported\n"};
    #else
        throw std::runtime_error{"Not compiled with GIF support"};
    #endif
    }
    else
    {
        throw std::runtime_error{"Unknown input file format\n"};
    }
}

void write_ascii(const Image & img,
                 const Char_vals & char_vals,
                 const std::string & output_filename,
                 int rows, int cols)
{
    std::ofstream output_file;
    if(output_filename != "-")
        output_file.open(output_filename);
    std::ostream & out = (output_filename == "-") ? std::cout : output_file;

    if(!out)
        throw std::runtime_error{"Could not open output file: " + std::string{std::strerror(errno)}};

    const auto px_col = static_cast<float>(img.get_width()) / cols;
    const auto px_row = rows > 0 ? static_cast<float>(img.get_height()) / rows : px_col * 2.0f;

    for(float row = 0.0f; row < img.get_height(); row += px_row)
    {
        for(float col = 0.0f; col < img.get_width(); col += px_col)
        {
            std::size_t pix_sum = 0;
            std::size_t cell_sum = 0;
            for(float y = row; y < row + px_row && y < img.get_height(); ++y)
            {
                for(float x = col; x < col + px_col && x < img.get_width(); ++x)
                {
                    pix_sum += img.get_pix(y, x);
                    ++cell_sum;
                }
            }
            out<<char_vals[pix_sum / cell_sum];
        }
        out<<'\n';
    }
}

int main(int argc, char * argv[])
{
    auto args = parse_args(argc, argv);
    if(!args)
        return EXIT_FAILURE;

    auto font_path = get_font_path(args->font_name);
    auto values = get_char_values(font_path, args->font_size);

    auto img = get_image_data(args->input_filename);
    write_ascii(*img, values, args->output_filename, args->rows, args->cols);

    return EXIT_SUCCESS;
}
