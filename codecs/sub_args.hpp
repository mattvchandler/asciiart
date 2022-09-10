#ifndef SUB_ARGS_HPP
#define SUB_ARGS_HPP

#include <string>
#include <vector>

#include "../cxxopts_wrapper.hpp"

class Sub_args: public cxxopts::Options
{
public:
    Sub_args(const std::string & group_name);
    cxxopts::OptionAdder add_options();
    cxxopts::ParseResult parse(const std::vector<std::string> & args);
    std::string help(const std::string & main_help) const;
private:
    std::string group_name_;
};

#endif // SUB_ARGS_HPP
