#ifndef MCMAP_HPP
#define MCMAP_HPP

#include "image.hpp"

#ifdef ZLIB_FOUND
class MCMap final: public Image
{
public:
    MCMap() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif
#endif // MCMAP_HPP
