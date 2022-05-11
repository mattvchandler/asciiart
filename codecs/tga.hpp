#ifndef TGA_HPP
#define TGA_HPP

#include "image.hpp"

class Tga final: public Image
{
public:
    Tga() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif // TGA_HPP
