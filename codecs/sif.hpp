#ifndef SIF_HPP
#define SIF_HPP

#include "image.hpp"

class Sif final: public Image
{
public:
    Sif(std::istream & input);
};
#endif // SIF_HPP
