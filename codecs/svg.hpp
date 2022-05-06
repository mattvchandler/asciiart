#ifndef SVG_HPP
#define SVG_HPP

#include "image.hpp"

#ifdef SVG_FOUND
class Svg final: public Image
{
public:
    Svg(std::istream & input, const std::string & filename, const Args & args);
};
#endif
#endif // SVG_HPP
