#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

class Xpm final: public Image
{
public:
    Xpm(const Header & header, std::istream & input, unsigned char bg);
};
#endif // XPM_HPP
