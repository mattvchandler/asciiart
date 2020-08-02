#ifndef TGA_HPP
#define TGA_HPP

#include "image.hpp"

class Tga final: public Image
{
public:
    explicit Tga(std::istream & input);
};
#endif // TGA_HPP
