#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

#ifdef XPM_FOUND
class Xpm final: public Image
{
public:
    Xpm(std::istream & input, unsigned char bg);
};
#endif
#endif // XPM_HPP
