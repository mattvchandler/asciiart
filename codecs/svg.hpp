#ifndef SVG_HPP
#define SVG_HPP

#include "image.hpp"

#ifdef SVG_FOUND
class Svg final: public Image
{
public:
    Svg() = default;
    void open(std::istream & input, const Args & args) override;
};
#endif
#endif // SVG_HPP
