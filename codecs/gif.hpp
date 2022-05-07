#ifndef GIF_HPP
#define GIF_HPP

#include "image.hpp"

inline bool is_gif(const Image::Header & header)
{
    const std::array<unsigned char, 6> gif_header1 = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61}; //GIF87a
    const std::array<unsigned char, 6> gif_header2 = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61}; //GIF89a

    return std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), Image::header_cmp);
}

#ifdef GIF_FOUND
class Gif final: public Image
{
public:
    Gif(std::istream & input, const Args & args);
    void handle_extra_args(const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);
private:
    bool count_ {false};
    unsigned int frame_{0};
    bool composed_ {true};
};
#endif
#endif // GIF_HPP
