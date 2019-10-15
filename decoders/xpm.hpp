#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

#ifdef HAS_XPM
class Xpm final: public Image
{
public:
    Xpm(std::istream & input, unsigned char bg);
};
#endif
#endif // XPM_HPP
