// Convert an image input to ascii art
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include <cmath>
#include <csetjmp>

#include <cxxopts.hpp>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef HAS_PNG
#include <png.h>
#endif

#ifdef HAS_JPEG
#include <jpeglib.h>
#endif

#ifdef HAS_GIF
#include <gif_lib.h>
#endif

struct Args
{
    std::string input_filename;  // - for stdin
    std::string output_filename; // - for stdout
    std::string font_name;       // use fontconfig to find, freetype to open
    float       font_size;       // font size requested, in points
    int rows;                    // output rows
    int cols;                    // output cols
    int bg;                      // BG color value
};

class Optional_pos: public cxxopts::Options
{
public:
    Optional_pos(const std::string & program, const std::string & help_string):
        cxxopts::Options{program, help_string}{}

    cxxopts::OptionAdder add_positionals()
    {
        return {*this, POS_GROUP_NAME};
    }
    void add_positionals(std::initializer_list<cxxopts::Option> options)
    {
        add_options(POS_GROUP_NAME, options);
    }
    cxxopts::ParseResult parse(int& argc, char**& argv)
    {
        std::string usage;

        for(auto && opt: group_help(POS_GROUP_NAME).options)
        {
            parse_positional(opt.l);

            if(opt.is_boolean)
                throw cxxopts::invalid_option_format_error{"Positional argument " + opt.l + "  must accept value"};

            if(!std::empty(usage))
                usage += " ";

            std::string usage_name = opt.arg_help.empty() ? opt.l : opt.arg_help;
            for(auto &&c: usage_name)
                c = std::toupper(c);

            if(opt.is_container)
                usage_name += "...";

            if(opt.has_default)
                usage += "[" + usage_name + "]";
            else
                usage += usage_name;
        }

        positional_help(usage);

        return cxxopts::Options::parse(argc, argv);
    }

    std::string help(const std::vector<std::string> & groups ={}, const std::string & msg = "")
    {
        auto txt = cxxopts::Options::help(groups);
        if(std::empty(groups))
        {
            std::size_t longest = 0;
            std::vector<std::pair<std::string, cxxopts::HelpOptionDetails>> pos;
            for(auto && opt: group_help(POS_GROUP_NAME).options)
            {
                std::string upper_name = opt.arg_help.empty() ? opt.l : opt.arg_help;
                for(auto &&c: upper_name)
                    c = std::toupper(c);

                upper_name = POS_HELP_INDENT + upper_name;

                longest = std::max(longest, std::size(upper_name));
                pos.emplace_back(upper_name, opt);
            }

            longest = std::min(longest, static_cast<size_t>(cxxopts::OPTION_LONGEST));
            auto allowed = size_t{76} - longest - cxxopts::OPTION_DESC_GAP;

            for(auto && [name, opt]: pos)
            {
                auto space = std::string(longest + cxxopts::OPTION_DESC_GAP - (std::size(name) > longest ? 0: std::size(name)), ' ');
                if(std::size(name) > longest)
                    space = '\n' + space;

                txt += POS_HELP_INDENT + name + space
                     + cxxopts::format_description(opt, longest + cxxopts::OPTION_DESC_GAP, allowed)
                     + '\n';
            }
        }

        if(!std::empty(msg))
            txt += '\n' + msg + '\n';

        return txt;
    }
    std::string help(const std::string & msg)
    {
        return help({}, msg);
    }

private:
    inline static const std::string POS_GROUP_NAME = "Positional";
    inline static const std::string POS_HELP_INDENT = "  ";
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[])
{
    Optional_pos options{argv[0], "Convert an image to ASCII art"};

    try
    {
        options.add_options()
            ("h,help",   "Show this message and quit")
            ("f,font",   "Font name to render. Uses fontconfig to find",                                  cxxopts::value<std::string>()->default_value("monospace"),  "FONT_PATTERN")
            ("s,size",   "Font size, in points",                                                          cxxopts::value<float>()->default_value("12.0"),             "")
            ("r,rows",   "# of output rows. Enter a negative value to preserve aspect ratio with --cols", cxxopts::value<int>()->default_value("-1"),                 "ROWS")
            ("c,cols",   "# of output cols",                                                              cxxopts::value<int>()->default_value("80"),                 "COLS")
            ("b,bg",     "Background color value for transparent images(0-255)",                          cxxopts::value<int>()->default_value("0"),                  "BG")
            ("o,output", "Output text file path. Output to stdout if '-'",                                cxxopts::value<std::string>()->default_value("-"),          "OUTPUT_FILE");

        options.add_positionals()
            ("input", "Input image path. Read from stdin if -", cxxopts::value<std::string>()->default_value("-"));

        auto args = options.parse(argc, argv);

        if(args.count("help"))
        {
            std::cerr<<options.help()<<'\n';
            return {};
        }

        if(args["rows"].as<int>() == 0)
        {
            std::cerr<<options.help("Value for --rows cannot be 0")<<'\n';
            return {};
        }
        if(args["cols"].as<int>() <= 0)
        {
            std::cerr<<options.help("Value for --cols must be positive")<<'\n';
            return {};
        }

        if(args["bg"].as<int>() < 0 || args["bg"].as<int>() > 255)
        {
            std::cerr<<options.help("Value for --bg must be within 0-255")<<'\n';
            return {};
        }

        return Args{
            args["input"].as<std::string>(),
            args["output"].as<std::string>(),
            args["font"].as<std::string>(),
            args["size"].as<float>(),
            args["rows"].as<int>(),
            args["cols"].as<int>(),
            args["bg"].as<int>(),
        };
    }
    catch(const cxxopts::OptionException & e)
    {
        std::cerr<<options.help(e.what())<<'\n';
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
    using Header = std::array<char, 12>;
};

#ifdef HAS_PNG
class Png final: public Image
{
public:
    Png(const Header & header, std::istream & input, int bg):
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

        if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

        if(bit_depth == 16)
            png_set_strip_16(png_ptr);

        if(bit_depth < 8)
            png_set_packing(png_ptr);

        auto number_of_passes = png_set_interlace_handling(png_ptr);

        png_read_update_info(png_ptr, info_ptr);

        if(png_get_rowbytes(png_ptr, info_ptr) != width_ * 2)
            throw std::runtime_error{"PNG bytes per row incorrect"};

        image_data_.resize(height_);
        for(auto && row: image_data_)
            row.resize(width_);

        // buffer for Gray + Alpha pixels
        std::vector<unsigned char> row_buffer(width_ * 2);

        for(decltype(number_of_passes) pass = 0; pass < number_of_passes; ++pass)
        {
            for(size_t row = 0; row < height_; ++row)
            {
                png_read_row(png_ptr, std::data(row_buffer), NULL);
                for(std::size_t col = 0; col < width_; ++col)
                {
                    // alpha blending w/ background
                    auto val   = row_buffer[col * 2]     / 255.0f;
                    auto alpha = row_buffer[col * 2 + 1] / 255.0f;

                    image_data_[row][col] = static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
                }
            }
        }

        std::cout<<(int)image_data_[0][0]<<'\n';

        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    }

    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    }

    size_t get_width() const override { return width_; }
    size_t get_height() const override { return height_; }
private:
    const Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};

    size_t width_{0};
    size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;

    void read_fn(png_bytep data, png_size_t length) noexcept
    {
        std::size_t png_ind = 0;
        while(header_bytes_read_ < std::size(header_) && png_ind < length)
            data[png_ind++] = header_[header_bytes_read_++];

        input_.read(reinterpret_cast<char *>(data) + png_ind, length - png_ind);
        if(input_.bad())
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
            std::cerr<<"FATAL ERROR: Could not get PNG struct pointer\n";
            std::exit(EXIT_FAILURE);
        }

        png->read_fn(data, length);
    }
};
#endif

#ifdef HAS_JPEG
class Jpeg final: public Image
{
public:
    Jpeg(const Header & header, std::istream & input)
    {
        jpeg_decompress_struct cinfo;
        my_jpeg_error jerr;

        cinfo.err = jpeg_std_error(&jerr);
        jerr.error_exit = my_jpeg_error::exit;

        my_jpeg_source source(header, input);

        jpeg_create_decompress(&cinfo);

        if(setjmp(jerr.setjmp_buffer))
        {
            jpeg_destroy_decompress(&cinfo);
            throw std::runtime_error{"Error reading with libjpg"};
        }

        cinfo.src = &source;

        jpeg_read_header(&cinfo, true);
        cinfo.out_color_space = JCS_GRAYSCALE;

        jpeg_start_decompress(&cinfo);

        width_ = cinfo.output_width;
        height_ = cinfo.output_height;

        image_data_.resize(height_);
        for(auto && row: image_data_)
            row.resize(width_);

        if(cinfo.output_width * cinfo.output_components != width_)
            throw std::runtime_error{"JPEG bytes per row incorrect"};

        while(cinfo.output_scanline < cinfo.output_height)
        {
            auto buffer = std::data(image_data_[cinfo.output_scanline]);
            jpeg_read_scanlines(&cinfo, &buffer, 1);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
    }

    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    }

    size_t get_width() const override { return width_; }
    size_t get_height() const override { return height_; }

private:

    struct my_jpeg_error: public jpeg_error_mgr
    {
        jmp_buf setjmp_buffer;
        static void exit(j_common_ptr cinfo) noexcept
        {
            cinfo->err->output_message(cinfo);
            std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);
        }
    };

    class my_jpeg_source: public jpeg_source_mgr
    {
    public:
        my_jpeg_source(const Header & header, std::istream & input):
            header_{header},
            input_{input}
        {
            init_source = [](j_decompress_ptr){};
            fill_input_buffer = my_fill_input_buffer;
            skip_input_data = my_skip_input_data;
            resync_to_restart = jpeg_resync_to_restart;
            term_source = [](j_decompress_ptr){};
            bytes_in_buffer = 0;
            next_input_byte = nullptr;
        }

    private:

        static boolean my_fill_input_buffer(j_decompress_ptr cinfo)
        {
            auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

            std::size_t jpeg_ind = 0;
            while(src.header_bytes_read_ < std::size(src.header_) && jpeg_ind < std::size(src.buffer_))
                src.buffer_[jpeg_ind++] = src.header_[src.header_bytes_read_++];

            src.input_.read(reinterpret_cast<char *>(std::data(src.buffer_)) + jpeg_ind, std::size(src.buffer_) - jpeg_ind);

            src.next_input_byte = std::data(src.buffer_);
            src.bytes_in_buffer = src.input_.gcount() + jpeg_ind;

            if(src.input_.bad() || src.bytes_in_buffer == 0)
            {
                std::cerr<<"ERROR: Could not read JPEG image\n";
                src.buffer_[0] = 0xFF;
                src.buffer_[1] = JPEG_EOI;
                src.bytes_in_buffer = 2;
            }

            return true;
        }
        static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
        {
            if(num_bytes > 0)
            {
                auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

                while(src.bytes_in_buffer < static_cast<decltype(src.bytes_in_buffer)>(num_bytes))
                {
                    num_bytes -= src.bytes_in_buffer;
                    my_fill_input_buffer(cinfo);
                }

                src.next_input_byte += num_bytes;
                src.bytes_in_buffer -= num_bytes;
            }
        }

        const Header & header_;
        std::istream & input_;
        std::array<JOCTET, 4096> buffer_;
        JOCTET * buffer_p_ { std::data(buffer_) };

        std::size_t header_bytes_read_ {0};
    };

    size_t width_{0};
    size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;
};
#endif

#ifdef HAS_GIF
class Gif final: public Image
{
public:
    Gif(const Header & header, std::istream & input, int bg):
        header_{header},
        input_{input}
    {
        int error_code = GIF_OK;
        GifFileType * gif = DGifOpen(this, read_fn, &error_code);
        if(!gif)
            throw std::runtime_error{"Error setting up GIF: " + std::string{GifErrorString(error_code)}};

        if(DGifSlurp(gif) != GIF_OK)
        {
            DGifCloseFile(gif, NULL);
            throw std::runtime_error{"Error reading GIF: " + std::string{GifErrorString(gif->Error)}};
        }

        auto pal = gif->SavedImages[0].ImageDesc.ColorMap;
        if(!pal)
        {
            pal = gif->SColorMap;
            if(!pal)
            {
                DGifCloseFile(gif, NULL);
                throw std::runtime_error{"Could not find color map"};
            }
        }

        std::vector<unsigned char> gray_pal(pal->ColorCount);
        for(std::size_t i = 0; i < std::size(gray_pal); ++i)
        {
            // formulas from https://www.w3.org/TR/WCAG20/
            std::array<float, 3> luminance_color = {
                pal->Colors[i].Red   / 255.0f,
                pal->Colors[i].Green / 255.0f,
                pal->Colors[i].Blue  / 255.0f
            };

            for(auto && c: luminance_color)
                c = (c <= 0.03928f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);

            auto luminance = 0.2126f * luminance_color[0] + 0.7152f * luminance_color[1] + 0.0722f * luminance_color[2];

            gray_pal[i] = luminance * 255;
        }

        int transparency_ind = -1;
        GraphicsControlBlock gcb;
        if(DGifSavedExtensionToGCB(gif, 0, &gcb) == GIF_OK)
            transparency_ind = gcb.TransparentColor;

        width_ = gif->SWidth;
        height_ = gif->SHeight;

        if(gif->SavedImages[0].ImageDesc.Left != 0 || gif->SavedImages[0].ImageDesc.Top != 0
                || static_cast<std::size_t>(gif->SavedImages[0].ImageDesc.Width) != width_
                || static_cast<std::size_t>(gif->SavedImages[0].ImageDesc.Height) != height_)
        {
            throw std::runtime_error{"GIF has wrong size or offset"};
        }

        auto & im = gif->SavedImages[0].RasterBits;

        image_data_.resize(height_);
        for(auto && row: image_data_)
        {
            row.resize(width_);
        }

        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                auto index = im[row * width_ + col];

                auto val = gray_pal[index];

                if(index == transparency_ind)
                    val = bg;

                image_data_[row][col] = val;
            }
        }

        DGifCloseFile(gif, NULL);
    }

    unsigned char get_pix(std::size_t row, std::size_t col) const override
    {
        return image_data_[row][col];
    }

    size_t get_width() const override { return width_; }
    size_t get_height() const override { return height_; }

private:
    const Header & header_;
    std::istream & input_;

    std::size_t header_bytes_read_ {0};

    int read_fn(GifByteType * data, int length) noexcept
    {
        std::size_t gif_ind = 0;
        while(header_bytes_read_ < std::size(header_) && gif_ind < static_cast<std::size_t>(length))
            data[gif_ind++] = header_[header_bytes_read_++];

        input_.read(reinterpret_cast<char *>(data) + gif_ind, length - gif_ind);
        if(input_.bad())
        {
            std::cerr<<"FATAL ERROR: Could not read GIF image\n";
            return GIF_ERROR;
        }

        return input_.gcount() + gif_ind;
    }

    static int read_fn(GifFileType* gif_file, GifByteType * data, int length) noexcept
    {
        auto gif = static_cast<Gif*>(gif_file->UserData);
        if(!gif)
        {
            std::cerr<<"FATAL ERROR: Could not get GIF struct pointer\n";
            return GIF_ERROR;
        }

        return gif->read_fn(data, length);
    }

    size_t width_{0};
    size_t height_{0};

    std::vector<std::vector<unsigned char>> image_data_;
};
#endif

[[nodiscard]] std::unique_ptr<Image> get_image_data(std::string & input_filename, int bg)
{
    std::ifstream input_file;
    if(input_filename != "-")
        input_file.open(input_filename, std::ios_base::in | std::ios_base::binary);
    std::istream & input = (input_filename == "-") ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file: " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));
    if(input.eof())
        throw std::runtime_error{"Could not read file header: not enough bytes"};
    else if(!input)
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
        return std::make_unique<Png>(header, input, bg);
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
        return std::make_unique<Jpeg>(header, input);
    #else
        throw std::runtime_error{"Not compiled with JPEG support"};
    #endif
    }
    else if(std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), header_cmp)
         || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), header_cmp))
    {
    #ifdef HAS_GIF
        return std::make_unique<Gif>(header, input, bg);
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

    try
    {
        auto font_path = get_font_path(args->font_name);
        auto values = get_char_values(font_path, args->font_size);

        auto img = get_image_data(args->input_filename, args->bg);
        write_ascii(*img, values, args->output_filename, args->rows, args->cols);
    }
    catch(const std::runtime_error & e)
    {
        std::cerr<<e.what()<<'\n';
    }

    return EXIT_SUCCESS;
}
