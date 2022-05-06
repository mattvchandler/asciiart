#ifndef PNM_HPP
#define PNM_HPP

#include "image.hpp"

inline bool is_pnm(const Image::Header & header)
{
    const std::array<unsigned char, 2> pbm_header   {'P', '1'};
    const std::array<unsigned char, 2> pgm_header   {'P', '2'};
    const std::array<unsigned char, 2> ppm_header   {'P', '3'};
    const std::array<unsigned char, 2> pbm_b_header {'P', '4'};
    const std::array<unsigned char, 2> pgm_b_header {'P', '5'};
    const std::array<unsigned char, 2> ppm_b_header {'P', '6'};
    const std::array<unsigned char, 2> pam_header   {'P', '7'};
    const std::array<unsigned char, 2> pfm_c_header {'P', 'F'};
    const std::array<unsigned char, 2> pfm_g_header {'P', 'f'};

    return std::equal(std::begin(pbm_header),   std::end(pbm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pgm_header),   std::end(pgm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(ppm_header),   std::end(ppm_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pbm_b_header), std::end(pbm_b_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pgm_b_header), std::end(pgm_b_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(ppm_b_header), std::end(ppm_b_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pam_header),   std::end(pam_header),   std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pfm_c_header), std::end(pfm_c_header), std::begin(header), Image::header_cmp)
        || std::equal(std::begin(pfm_g_header), std::end(pfm_g_header), std::begin(header), Image::header_cmp);
}

class Pnm final: public Image
{
public:
    Pnm(std::istream & input, const Args & args);

    static void write_pbm(std::ostream & path, const Image & image, unsigned char bg, bool invert);
    static void write_pgm(std::ostream & path, const Image & image, unsigned char bg, bool invert);
    static void write_ppm(std::ostream & path, const Image & image, unsigned char bg, bool invert);
    static void write_pam(std::ostream & path, const Image & image, bool invert);
    static void write_pfm(std::ostream & path, const Image & image, unsigned char bg, bool invert);

private:
    void read_P1(std::istream & input);
    void read_P2(std::istream & input);
    void read_P3(std::istream & input);
    void read_P4(std::istream & input);
    void read_P5(std::istream & input);
    void read_P6(std::istream & input);
    void read_P7(std::istream & input);
    void read_PF_color(std::istream & input);
    void read_PF_gray(std::istream & input);
};
#endif // PNM_HPP
