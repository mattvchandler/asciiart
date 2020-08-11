#ifndef PCX_HPP
#define PCX_HPP

#include "image.hpp"

class Pcx final: public Image
{
public:
    explicit Pcx(std::istream & input);

    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif // PCX_HPP
