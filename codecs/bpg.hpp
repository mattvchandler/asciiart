#ifndef BPG_HPP
#define BPG_HPP

#include "image.hpp"

#include <algorithm>

inline bool is_bpg(const Image::Header & header)
{
    const std::array<unsigned char, 4> bpg_header = {'B', 'P', 'G', 0xFB};
    return std::equal(std::begin(bpg_header), std::end(bpg_header), std::begin(header), Image::header_cmp);
}

#ifdef BPG_FOUND
class Bpg final: public Image
{
public:
    Bpg() = default;
    void open(std::istream & input, const Args & args) override;
};
#endif
#endif // BPG_HPP
