#ifndef SIF_HPP
#define SIF_HPP

#include "image.hpp"

class Sif final: public Image
{
public:
    Sif() = default;
    void open(std::istream & input, const Args & args) override;
};
#endif // SIF_HPP
