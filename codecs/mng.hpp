#ifndef MNG_HPP
#define MNG_HPP

#include "image.hpp"

inline bool is_mng(const Image::Header & header)
{
    const std::array<unsigned char, 8> mng_header = {0x8A, 0x4D, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const std::array<unsigned char, 8> jng_header = {0x8B, 0x4A, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    return std::equal(std::begin(mng_header), std::end(mng_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(jng_header), std::end(jng_header), std::begin(header), Image::header_cmp);
}

#ifdef MNG_FOUND
class Mng final: public Image
{
public:
    Mng() = default;
    void open(std::istream & input, const Args & args) override;

    bool supports_multiple_images() const override { return true; }
    bool supports_animation() const override { return true; }
};
#endif
#endif // MNG_HPP
