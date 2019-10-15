#ifndef SVG_HPP
#define SVG_HPP

#include "image.hpp"

#ifdef HAS_SVG
class Svg final: public Image
{
public:
    Svg(std::istream & input, const std::string & filename, unsigned char bg);
};
#endif
#endif // SVG_HPP
