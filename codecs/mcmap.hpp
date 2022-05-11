#ifndef MCMAP_HPP
#define MCMAP_HPP

#include "image.hpp"

#ifdef ZLIB_FOUND
class MCMap final: public Image
{
public:
    MCMap(std::istream & input);
    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif
#endif // MCMAP_HPP
