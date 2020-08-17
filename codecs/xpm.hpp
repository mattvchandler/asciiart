#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

#ifdef XPM_FOUND
class Xpm final: public Image
{
public:
    explicit Xpm(std::istream & input);

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // XPM_HPP
