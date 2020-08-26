#include "args.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

#include <cxxopts.hpp>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

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
    cxxopts::ParseResult parse(int& argc, const char**& argv)
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
        std::vector<std::string> pos_last_groups = groups;

        if(std::empty(pos_last_groups))
            pos_last_groups = cxxopts::Options::groups();

        auto pos_i = std::find(std::begin(pos_last_groups), std::end(pos_last_groups), POS_GROUP_NAME);
        bool has_pos = pos_i != std::end(pos_last_groups);
        if(has_pos)
        {
            pos_last_groups.erase(pos_i);
            pos_last_groups.push_back(POS_GROUP_NAME);
        }

        auto txt = cxxopts::Options::help(pos_last_groups);

        if(has_pos)
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

static const std::vector<std::string> input_formats =
{
    #ifdef AVIF_FOUND
    "AVIF",
    #endif
    "BMP",
    #ifdef BPG_FOUND
    "BPG",
    #endif
    "CUR","ICO",
    #ifdef FLIF_ENC_FOUND
    "FLIF",
    #endif
    #ifdef GIF_FOUND
    "GIF",
    #endif
    #ifdef HEIF_FOUND
    "HEIF",
    #endif
    #ifdef JPEG_FOUND
    "JPEG",
    #endif
    #ifdef JP2_FOUND
    "JPEG 2000",
    #endif
    #ifdef OpenEXR_FOUND
    "OpenEXR",
    #endif
    "PCX",
    #ifdef PNG_FOUND
    "PNG",
    #endif
    "PBM", "PGM", "PPM", "PAM", "PFM",
    "SIF",
    #ifdef TIFF_FOUND
    "TIFF",
    #endif
    #ifdef WEBP_FOUND
    "WebP",
    #endif
    #ifdef XPM_FOUND
    "XPM",
    #endif
    "TGA"
};

static const std::vector<std::string> output_formats =
{
    #ifdef AVIF_FOUND
    ".avif",
    #endif
    ".bmp",
    ".cur",".ico",
    #ifdef OpenEXR_FOUND
    ".exr",
    #endif
    #ifdef FLIF_ENC_FOUND
    ".flif",
    #endif
    #ifdef GIF_FOUND
    ".gif",
    #endif
    #ifdef HEIF_FOUND
    ".heif",
    #endif
    #ifdef JPEG_FOUND
    ".jpg", ".jpeg",
    #endif
    #ifdef JP2_FOUND
    ".jp2",
    #endif
    ".pcx",
    #ifdef PNG_FOUND
    ".png",
    #endif
    ".pbm", ".pgm", ".ppm", ".pam", ".pfm",
    ".tga",
    #ifdef TIFF_FOUND
    ".tif",
    #endif
    #ifdef WEBP_FOUND
    ".webp",
    #endif
    #ifdef XPM_FOUND
    ".xpm",
    #endif
};

[[nodiscard]] std::optional<Args> parse_args(int argc, char * argv[])
{
    Optional_pos options{argv[0], "Display an image in the terminal, with ANSI colors and/or ASCII art"};

    std::string input_format_list;
    for(std::size_t i = 0; i < std::size(input_formats); ++i)
    {
        if(i > 0)
            input_format_list += ", ";
        input_format_list += input_formats[i];
    }

    std::string output_format_list;
    for(std::size_t i = 0; i < std::size(output_formats); ++i)
    {
        if(i > 0)
            output_format_list += ", ";
        output_format_list += output_formats[i].substr(1);
    }

    try
    {
        options.add_options()
            ("h,help",    "Show this message and quit")
            ("r,rows",    "# of output rows. Enter a negative value to preserve aspect ratio with --cols", cxxopts::value<int>()->default_value("-1"),        "ROWS")
            ("c,cols",    "# of output cols",                                                              cxxopts::value<int>()->default_value("80"),        "COLS")
            ("b,bg",      "Background color value for transparent images (0-255)",                         cxxopts::value<int>()->default_value("0"),         "BG")
            ("i,invert",  "Invert colors")
            ("o,output",  "Output text file path. Output to stdout if '-'",                                cxxopts::value<std::string>()->default_value("-"), "OUTPUT_FILE")
            ("v,convert", "Convert input to output file. Supported formats: " + output_format_list,        cxxopts::value<std::string>(),                     "OUTPUT_IMAGE_FILE");

        #if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
        const std::string font_group = "Text display options";
        options.add_options(font_group)
            ("f,font",    "Font name to render. Uses fontconfig to find",cxxopts::value<std::string>()->default_value("monospace"), "FONT_PATTERN")
            ("s,size",    "Font size, in points",                        cxxopts::value<float>()->default_value("12.0"),            "");
        #endif

        const std::string color_group = "Color";
        options.add_options(color_group)
            ("ansi4",     "use 4-bit ANSI colors")
            ("ansi8",     "use 8-bit ANSI colors")
            ("ansi24",    "use 24-bit ANSI colors. Default when output is stdout to terminal")
            ("nocolor",   "disable colors. Default when output is not stdout to terminal")

            ("halfblock", "use unicode half-block to display 2 colors per character. Enabled automatically unless overridden by --ascii or --space. Use --space instead if your terminal has problems with unicode output")
            ("ascii",     "use ascii chars for display. More dense chars will be used for higher luminosity colors. Enabled automatically when --nocolor set")
            ("space",     "use spaces for display. Not allowed when --ascii set");

        const std::string filetype_group = "Input file detection override (for formats that can't reliably be identified by file signature)";
        options.add_options(filetype_group)("tga", "Interpret input as a TGA file");
        options.add_options(filetype_group)("pcx", "Interpret input as a PCX file");

    #ifdef SVG_FOUND
        options.add_options(filetype_group)("svg", "Interpret input as an SVG file");
    #endif
    #ifdef XPM_FOUND
        options.add_options(filetype_group)("xpm", "Interpret input as an XPM file");
    #endif

        options.add_options(filetype_group)("sif", "Interpret input as a Space Image Format file (from Advent of Code 2019)");

        options.add_positionals()
            ("input", "Input image path. Read from stdin if -. Supported formats: " + input_format_list, cxxopts::value<std::string>()->default_value("-"));

        auto args = options.parse(argc, const_cast<const char **&>(argv));

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

        if(args.count("ansi4") + args.count("ansi8") + args.count("ansi24") + args.count("nocolor") > 1)
        {
            std::cerr<<options.help("Only one color option flag may be specified")<<'\n';
            return {};
        }

        auto color {Args::Color::NONE};
        if(args.count("ansi4"))
        {
            color = Args::Color::ANSI4;
        }
        else if(args.count("ansi8"))
        {
            color = Args::Color::ANSI8;
        }
        else if(args.count("ansi24"))
        {
            color = Args::Color::ANSI24;
        }
        else if(args.count("nocolor"))
        {
            color = Args::Color::NONE;
        }
        else if(args["output"].as<std::string>() == "-")
        {
        #ifdef HAS_UNISTD
            color = isatty(fileno(stdout)) ? Args::Color::ANSI24 : Args::Color::NONE;
        #else
            color = Args::Color::NONE;
        #endif
        }

        if(args.count("halfblock") + args.count("space") + (args.count("ascii") || color == Args::Color::NONE ? 1 : 0) > 1)
        {
            std::cerr<<options.help("Only one of --halfblock, --ascii, or --space may be specified")<<'\n';
            return {};
        }

        auto disp_char {Args::Disp_char::HALF_BLOCK};
        if(args.count("ascii") || color == Args::Color::NONE)
            disp_char = Args::Disp_char::ASCII;
        else if(args.count("space"))
            disp_char = Args::Disp_char::SPACE;

        auto filetype {Args::Force_file::detect};

        if(args.count("tga")
                + args.count("pcx")
    #ifdef SVG_FOUND
                + args.count("svg")
    #endif
    #ifdef XPM_FOUND
                + args.count("xpm")
    #endif
                + args.count("sif")
                > 1)
        {
            std::cerr<<options.help("Only one file format flag may be specified")<<'\n';
            return {};
        }

        if(args.count("tga"))
            filetype = Args::Force_file::tga;
        else if(args.count("pcx"))
            filetype = Args::Force_file::pcx;
    #ifdef SVG_FOUND
        else if(args.count("svg"))
            filetype = Args::Force_file::svg;
    #endif
    #ifdef XPM_FOUND
        else if(args.count("xpm"))
            filetype = Args::Force_file::xpm;
    #endif
        else if(args.count("sif"))
            filetype = Args::Force_file::aoc_2019_sif;

        std::optional<std::pair<std::string, std::string>> convert_path {};
        if(args.count("convert"))
        {
            convert_path.emplace();
            auto && [path, ext] = *convert_path;

            path = args["convert"].as<std::string>();

            // TODO: if std::filesystem is ever reliably supported across compilers / platforms without needing to link to an extra library, use that to get the extension instead
            for(std::size_t i = std::size(path); i-- > 0;)
            {
                if(path[i] == '.')
                {
                    ext = path.substr(i);
                    break;
                }
                else if(path[i] == '/' || path[i] == '\\')
                    break;
            }

            if(std::size(ext) == 0)
            {
                std::cerr<<options.help("No conversion type specified");
                return {};
            }

            for(auto && i: ext)
                i = std::tolower(i);

            if(std::find(std::begin(output_formats), std::end(output_formats), ext) == std::end(output_formats))
            {
                std::cerr<<options.help("Unsupported conversion type: " + ext);
                return {};
            }
        }

        return Args{
            args["input"].as<std::string>(),
            args["output"].as<std::string>(),
        #if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
            args["font"].as<std::string>(),
            args["size"].as<float>(),
        #else
            {},{},
        #endif
            args["rows"].as<int>(),
            args["cols"].as<int>(),
            static_cast<unsigned char>(args["bg"].as<int>()),
            static_cast<bool>(args.count("invert")),
            color,
            disp_char,
            filetype,
            convert_path
        };
    }
    catch(const cxxopts::OptionException & e)
    {
        std::cerr<<options.help(e.what())<<'\n';
        return {};
    }
}
