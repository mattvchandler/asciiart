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
    Srf() = default;
    void open(std::istream & input, const Args & args) override;

    void handle_extra_args(const Args & args) override;
    bool supports_animation() const override { return supports_animation_; }
    bool supports_multiple_images() const override { return supports_animation_; }
    bool supports_subimages() const override { return true; }

    std::chrono::duration<float> get_frame_delay(std::size_t) const override;

private:
    bool supports_animation_ {true};
    bool mosaic_ {false};
};
#endif // SRF_HPP
