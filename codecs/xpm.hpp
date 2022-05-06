#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

#ifdef XPM_FOUND
class Xpm final: public Image
{
public:
    Xpm(std::istream & input, const Args & args);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // XPM_HPP
