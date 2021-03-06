#ifndef TGA_HPP
#define TGA_HPP

#include "image.hpp"

class Tga final: public Image
{
public:
    explicit Tga(std::istream & input);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif // TGA_HPP
