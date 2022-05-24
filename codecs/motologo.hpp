#ifndef MOTOLOGO_HPP
#define MOTOLOGO_HPP

#include "image.hpp"

inline bool is_motologo(const Image::Header & header)
{
    const std::array<unsigned char, 9> srf_header = {'M', 'o', 't', 'o', 'L', 'o', 'g', 'o', '\0'};

    return std::equal(std::begin(srf_header), std::end(srf_header), std::begin(header), Image::header_cmp);
}

class MotoLogo final: public Image
{
public:
    MotoLogo() = default;
    void open(std::istream & input, const Args & args) override;

    void handle_extra_args(const Args & args) override;
    bool supports_multiple_images() const override { return true; }

private:
    bool list_ {false};
    std::optional<std::string> image_name_;
};
#endif // MOTOLOGO_HPP
