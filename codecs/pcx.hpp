#ifndef PCX_HPP
#define PCX_HPP

#include "image.hpp"

class Pcx final: public Image
{
public:
    Pcx() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, unsigned char bg, bool invert);
};
#endif // PCX_HPP
