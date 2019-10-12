#ifndef TGA_HPP
#define TGA_HPP

#include "image.hpp"

class Tga final: public Image
{
public:
    Tga(const Header & header, std::istream & input, unsigned char bg);
};
#endif // TGA_HPP
