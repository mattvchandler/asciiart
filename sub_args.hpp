#ifndef SUB_ARGS_HPP
#define SUB_ARGS_HPP

#include <string>

#include <cxxopts.hpp>

class Sub_args: public cxxopts::Options
{
public:
    Sub_args(const std::string & group_name):
        cxxopts::Options{""},
        group_name_{group_name}
    {}
    cxxopts::OptionAdder add_options()
    {
        return cxxopts::Options::add_options(group_name_);
    }
    cxxopts::ParseResult parse(const std::vector<std::string> & args)
    {
        auto argv = std::vector<const char *>{};
        argv.emplace_back("");
        for(auto && i: args)
            argv.emplace_back(i.c_str());

        return cxxopts::Options::parse(std::size(argv), std::data(argv));

    }
    std::string help(const std::string & main_help)
    {
        auto sub_help = cxxopts::Options::help();
        constexpr auto newline_count = 4u;
        auto count = 0u, pos = 0u;
        for(; pos < std::size(sub_help) && count < newline_count; ++pos)
        {
            if(sub_help[pos] == '\n')
                ++count;
        }
        return main_help + "\n" + sub_help.substr(pos);
    }
private:
    std::string group_name_;
};

#endif // SUB_ARGS_HPP
