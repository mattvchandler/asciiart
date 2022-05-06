#ifndef SRF_HPP
#define SRF_HPP

#include "image.hpp"

inline bool is_srf(const Image::Header & header)
{
    const std::array<unsigned char, 12> srf_header = {'G', 'A', 'R', 'M', 'I', 'N', ' ', 'B', 'I', 'T', 'M', 'A'}; // technically, the header is 16 bytes, but this is probably enough.

    return std::equal(std::begin(srf_header), std::end(srf_header), std::begin(header), Image::header_cmp);
}

class Srf final: public Image
{
public:
    Srf(std::istream & input, const Args & args);
};
#endif // SRF_HPP
