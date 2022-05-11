#ifndef XPM_HPP
#define XPM_HPP

#include "image.hpp"

#ifdef XPM_FOUND
class Xpm final: public Image
{
public:
    Xpm() = default;
    void open(std::istream & input, const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);
};
#endif
#endif // XPM_HPP
