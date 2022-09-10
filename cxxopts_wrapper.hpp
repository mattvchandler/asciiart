#ifndef CXXOPTS_WRAPPER_HPP
#define CXXOPTS_WRAPPER_HPP

#include <cxxopts.hpp>

// the following addresses a breaking change in the 3.0 branch of cxxopts that
// isn't detectable by version number.
// prior versions use cxxopts::OptionException, later use cxxopts::exceptions::exception
// this should detect whichever is available and use that, falling back to std::exception

// (incomplete) forward declarations
namespace cxxopts
{
    struct OptionException;
    namespace exceptions
    {
        struct exception;
    }
}

namespace detail
{
    // is_type_complete technique from Raymond Chen https://devblogs.microsoft.com/oldnewthing/20190711-00/?p=102682
    template<typename, typename = void>
    inline constexpr bool is_type_complete_v = false;

    template<typename T>
    inline constexpr bool is_type_complete_v <T, std::void_t<decltype(sizeof(T))>> = true;

    template <typename>
    struct cxxopt_exception_detect
    {
        using type = std::exception;
    };

    template <typename T> requires is_type_complete_v<cxxopts::OptionException>
    struct cxxopt_exception_detect<T>
    {
        using type = cxxopts::OptionException;
    };

    template <typename T> requires is_type_complete_v<cxxopts::exceptions::exception>
    struct cxxopt_exception_detect<T>
    {
        using type = cxxopts::exceptions::exception;
    };
}

using cxxopt_exception = detail::cxxopt_exception_detect<void>::type;

#endif // CXXOPTS_WRAPPER_HPP
