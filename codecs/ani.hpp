#ifndef AVI_HPP
#define AVI_HPP

#include "image.hpp"

inline bool is_ani(const Image::Header & header)
{
    const std::array<unsigned char, 4> riff_header = {'R', 'I', 'F', 'F'};
    const std::array<unsigned char, 4> acon_header = {'A', 'C', 'O', 'N'};

    return std::equal(std::begin(riff_header), std::end(riff_header), std::begin(header), Image::header_cmp) &&
           std::equal(std::begin(acon_header), std::end(acon_header), std::begin(header) + 8, Image::header_cmp);
}

class Ani final: public Image
{
public:
    Ani() = default;
    void open(std::istream & input, const Args & args) override;

    bool supports_multiple_images() const override { return true; }
    bool supports_animation() const override { return true; }
};

#endif // AVI_HPP
