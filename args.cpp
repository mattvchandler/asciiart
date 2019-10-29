#include "args.hpp"

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
            ("i,invert", "Invert colors")
            ("o,output", "Output text file path. Output to stdout if '-'",                                cxxopts::value<std::string>()->default_value("-"),          "OUTPUT_FILE");

        const std::string color_group = "Color";
        options.add_options(color_group)
            ("ansi4",   "use 4-bit ANSI colors")
            ("ansi8",   "use 8-bit ANSI colors")
            ("ansi24",  "use 24-bit ANSI colors. Default when output is stdout to terminal")
            ("nocolor", "disable colors. Default when output is not stdout to terminal")
            ("ascii",   "use ascii chars instead of colored backgrounds. Enabled when --nocolor set");

        const std::string filetype_group = "Input file detection override";
        options.add_options(filetype_group)("tga", "Interpret input as a TGA file");

    #ifdef SVG_FOUND
        options.add_options(filetype_group)("svg", "Interpret input as an SVG file");
    #endif
    #ifdef XPM_FOUND
        options.add_options(filetype_group)("xpm", "Interpret input as an XPM file");
    #endif

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

        auto filetype {Args::Force_file::detect};

        if(args.count("tga")
    #ifdef SVG_FOUND
                + args.count("svg")
    #endif
    #ifdef XPM_FOUND
                + args.count("xpm")
    #endif
                > 1)
        {
            std::cerr<<options.help("Only one file format flag may be specified")<<'\n';
            return {};
        }

        if(args.count("tga"))
            filetype = Args::Force_file::tga;
    #ifdef SVG_FOUND
        else if(args.count("svg"))
            filetype = Args::Force_file::svg;
    #endif
    #ifdef XPM_FOUND
        else if(args.count("xpm"))
            filetype = Args::Force_file::xpm;
    #endif

        return Args{
            args["input"].as<std::string>(),
            args["output"].as<std::string>(),
            args["font"].as<std::string>(),
            args["size"].as<float>(),
            args["rows"].as<int>(),
            args["cols"].as<int>(),
            static_cast<unsigned char>(args["bg"].as<int>()),
            static_cast<bool>(args.count("invert")),
            color,
            args.count("ascii") || color == Args::Color::NONE,
            filetype,
        };
    }
    catch(const cxxopts::OptionException & e)
    {
        std::cerr<<options.help(e.what())<<'\n';
        return {};
    }
}
