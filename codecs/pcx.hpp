#ifndef PCX_HPP
#define PCX_HPP

#include "image.hpp"

class Pcx final: public Image
{
public:
    Pcx(std::istream & input, const Args & args);

    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif // PCX_HPP
