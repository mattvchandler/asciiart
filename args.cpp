#include "args.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

#include <cstdlib>

#include <cxxopts.hpp>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif
#ifdef HAS_IOCTL
#include <sys/ioctl.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#endif
#ifdef HAS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
                    #if CXXOPTS__VERSION_MAJOR >= 3
                    + cxxopts::format_description(opt, longest + cxxopts::OPTION_DESC_GAP, allowed, false)
                    #else
                    + cxxopts::format_description(opt, longest + cxxopts::OPTION_DESC_GAP, allowed)
                    #endif
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
    #ifdef JXL_FOUND
    "JPEG XL",
    #endif
    "Moto image.bin",
    #ifdef ZLIB_FOUND
    "Minecraft Map Item (.dat)",
    #endif
    #ifdef OpenEXR_FOUND
    "OpenEXR",
    #endif
    "PCX",
    #ifdef PNG_FOUND
    "PNG",
    #endif
    "PBM", "PGM", "PPM", "PAM", "PFM",
    "SRF",
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
    #ifdef ZLIB_FOUND
    ".dat",
    #endif
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
    #ifdef JXL_FOUND
    ".jxl",
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
    auto prog_name = std::string{argv[0]};
    if(auto sep_pos = prog_name.find_last_of("\\/"); sep_pos != std::string::npos)
        prog_name = prog_name.substr(sep_pos + 1);

    Optional_pos options{prog_name, "Display an image in the terminal, with ANSI colors and/or ASCII art"};

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
            ("h,help",     "Show this message and quit")
            ("r,rows",     "# of output rows. Enter a negative value to preserve aspect ratio with --cols", cxxopts::value<int>()->default_value("-1"),        "ROWS")
            ("c,cols",     "# of output cols",                                                              cxxopts::value<int>()->default_value("80"),        "COLS")
            ("b,bg",       "Background color value for transparent images (0-255)",                         cxxopts::value<int>()->default_value("0"),         "BG")
            ("i,invert",   "Invert colors")
            ("o,output",   "Output text file path. Output to stdout if '-'",                                cxxopts::value<std::string>()->default_value("-"), "OUTPUT_FILE")
            ("v,convert",  "Convert input to output file. Supported formats: " + output_format_list,        cxxopts::value<std::string>(),                     "OUTPUT_IMAGE_FILE")
            ("no-display", "Disable display of image");

        #if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
        const std::string font_group = "Text display options";
        options.add_options(font_group)
            ("f,font",    "Font name to render. Uses fontconfig to find", cxxopts::value<std::string>()->default_value("monospace"), "FONT_PATTERN")
            ("s,size",    "Font size, in points",                         cxxopts::value<float>()->default_value("12.0"),            "");
        #endif

        const std::string color_group = "Color";
        options.add_options(color_group)
            ("ansi4",     "Use 4-bit ANSI colors")
            ("ansi8",     "Use 8-bit ANSI colors")
            ("ansi24",    "Use 24-bit ANSI colors. Default when output is stdout to terminal")
            ("nocolor",   "Disable colors. Default when output is not stdout to terminal")

            ("halfblock", "Use unicode half-block to display 2 colors per character. Enabled automatically unless overridden by --ascii or --space. Use --space instead if your terminal has problems with unicode output")
            ("ascii",     "Use ascii chars for display. More dense chars will be used for higher luminosity colors. Enabled automatically when --nocolor set")
            ("space",     "Use spaces for display. Not allowed when --ascii set");

        const std::string multi_group = "Multiple image / animation (where input format support exists)";
        options.add_options(multi_group)
            ("image-no",    "Get specified image or frame number", cxxopts::value<unsigned int>(), "IMAGE_NO")
            ("image-count", "Print number of images / frames and exit")
            ("animate",     "Animate image (implies --no-display)")
            ("loop",        "Loop animation (implies --animation")
            ("frame-delay", "Animation delay between frames (in seconds). If not specified, get from image", cxxopts::value<float>(), "FRAME_DELAY")
            ("framerate",   "Animation framerate (in fps). If not specified, get from image", cxxopts::value<float>(), "FPS");

        const std::string filetype_group = "Input file detection override (for formats that can't reliably be identified by file signature)";
        options.add_options(filetype_group)("tga", "Interpret input as a TGA file");
        options.add_options(filetype_group)("pcx", "Interpret input as a PCX file");

    #ifdef SVG_FOUND
        options.add_options(filetype_group)("svg", "Interpret input as an SVG file");
    #endif
    #ifdef XPM_FOUND
        options.add_options(filetype_group)("xpm", "Interpret input as an XPM file");
    #endif
    #ifdef ZLIB_FOUND
        options.add_options(filetype_group)("mcmap", "Interpret input as an Minecraft Map Item .dat file");
    #endif

        options.add_options(filetype_group)("sif", "Interpret input as a Space Image Format file (from Advent of Code 2019)");

        options.add_positionals()
            ("input", "Input image path. Read from stdin if -. Supported formats: " + input_format_list, cxxopts::value<std::string>()->default_value("-"));

        options.allow_unrecognised_options();

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

        auto cols = args["cols"].as<int>();
        if(args["cols"].has_default())
        {
            if(auto columns_env = std::getenv("COLUMNS"); columns_env != nullptr)
                cols = std::min(cols, std::stoi(std::string{columns_env}));
            #ifdef HAS_IOCTL
            else if(winsize ws; ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0)
                cols = std::min(cols, static_cast<int>(ws.ws_col));
            #endif
            #ifdef HAS_WINDOWS
            else if(CONSOLE_SCREEN_BUFFER_INFO csbi; GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
                cols = std::min(cols, static_cast<int>(csbi.srWindow.Right - csbi.srWindow.Left + 1));
            #endif
        }
        if(cols <= 0)
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

        bool animate = args.count("animate");
        if(args.count("loop"))
            animate = true;

        if(animate && args.count("image-no"))
        {
            std::cerr<<options.help("Can't specify --image-no with --animate")<<'\n';
            return {};
        }
        if(animate && args.count("image-count"))
        {
            std::cerr<<options.help("Can't specify --image-count with --animate")<<'\n';
            return {};
        }

        if(animate && args.count("convert"))
        {
            std::cerr<<options.help("Can't specify --convert with --animate")<<'\n';
            return {};
        }

        auto frame_delay = args.count("frame-delay") ? args["frame-delay"].as<float>() : 0.0f;
        if(args.count("framerate"))
        {
            auto framerate = args["framerate"].as<float>();
            if(framerate > 0.0f)
                frame_delay = 1.0f / framerate;
            else
            {
                std::cerr<<options.help("--framerate must be > 0")<<'\n';
                return {};
            }
        }

        auto filetype {Args::Force_file::detect};

        if(args.count("tga")
                + args.count("pcx")
    #ifdef SVG_FOUND
                + args.count("svg")
    #endif
    #ifdef XPM_FOUND
                + args.count("xpm")
    #endif
    #ifdef ZLIB_FOUND
                + args.count("mcmap")
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
    #ifdef ZLIB_FOUND
        else if(args.count("mcmap"))
            filetype = Args::Force_file::mcmap;
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
            .input_filename        = args["input"].as<std::string>(),
            .output_filename       = args["output"].as<std::string>(),
        #if defined(FONTCONFIG_FOUND) && defined(FREETYPE_FOUND)
            .font_name             = args["font"].as<std::string>(),
            .font_size             = args["size"].as<float>(),
        #else
            .font_name             = {},
            .font_size             = {},
        #endif
            .rows                  = args["rows"].as<int>(),
            .cols                  = cols,
            .bg                    = static_cast<unsigned char>(args["bg"].as<int>()),
            .invert                = static_cast<bool>(args.count("invert")),
            .display               = animate ? false : !static_cast<bool>(args.count("no-display")),
            .color                 = color,
            .disp_char             = disp_char,
            .force_file            = filetype,
            .convert_filename      = convert_path,
            .image_no              = args.count("image-no") ? std::optional(args["image-no"].as<unsigned int>()) : std::nullopt,
            .get_image_count       = static_cast<bool>(args.count("image-count")),
            .animate               = animate,
            .loop_animation        = static_cast<bool>(args.count("loop")),
            .animation_frame_delay = frame_delay,
            .extra_args            = args.unmatched(),
            .help_text             = options.help()
        };
    }
    catch(const cxxopts::OptionException & e)
    {
        std::cerr<<options.help(e.what())<<'\n';
        return {};
    }
}
