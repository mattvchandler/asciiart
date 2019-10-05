#include "image.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <cmath>
#include <cstring>

#ifdef HAS_PNG
#include "png.hpp"
#endif

#ifdef HAS_JPEG
#include "jpeg.hpp"
#endif

#ifdef HAS_GIF
#include "gif.hpp"
#endif

#include "pnm.hpp"

unsigned char rgb_to_gray(unsigned char r, unsigned char g, unsigned char b)
{
    return rgb_to_gray(r / 255.0f, g / 255.0f, b / 255.0f);
}
float rgb_to_gray(float r, float g, float b)
{
    // formulas from https://www.w3.org/TR/WCAG20/
    std::array<float, 3> luminance_color = { r, g, b };

    for(auto && c: luminance_color)
        c = (c <= 0.03928f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);

    auto luminance = 0.2126f * luminance_color[0] + 0.7152f * luminance_color[1] + 0.0722f * luminance_color[2];

    return static_cast<unsigned char>(luminance * 255.0f);
}

Header_buf::Header_buf(const Image::Header & header, std::istream & input): input_{input}
{
    static_assert(buffer_size >= std::size(Image::Header()));

    std::copy(std::begin(header), std::end(header), std::begin(buffer_));
    input_.read(std::data(buffer_) + std::size(header), std::size(buffer_) - std::size(header));

    auto buffer_size = input_.gcount() + std::size(header);
    setg(std::data(buffer_), std::data(buffer_), std::data(buffer_) + buffer_size);
}

int Header_buf::underflow()
{
    input_.read(std::data(buffer_), std::size(buffer_));
    if(input_.bad())
        return traits_type::eof();

    auto buffer_size = input_.gcount();

    if(buffer_size == 0)
        return traits_type::eof();

    setg(std::data(buffer_), std::data(buffer_), std::data(buffer_) + buffer_size);

    return buffer_[0];
}

bool header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

bool is_png(Image::Header & header)
{
    const std::array<unsigned char, 8> png_header = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return std::equal(std::begin(png_header), std::end(png_header), std::begin(header), header_cmp);
}

bool is_jpeg(Image::Header & header)
{
    const std::array<unsigned char, 4>  jpeg_header1   = {0XFF, 0xD8, 0xFF, 0xDB};
    const std::array<unsigned char, 12> jpeg_header2   = {0XFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01};
    const std::array<unsigned char, 4>  jpeg_header3   = {0XFF, 0xD8, 0xFF, 0xEE};
    const std::array<unsigned char, 4>  jpeg_header4_1 = {0XFF, 0xD8, 0xFF, 0xE1};
    const std::array<unsigned char, 6>  jpeg_header4_2 = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00}; // there are 2 "don't care" bytes between these

    return std::equal(std::begin(jpeg_header1),   std::end(jpeg_header1),   std::begin(header), header_cmp)
       ||  std::equal(std::begin(jpeg_header2),   std::end(jpeg_header2),   std::begin(header), header_cmp)
       ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
       ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
       || (std::equal(std::begin(jpeg_header4_1), std::end(jpeg_header4_1), std::begin(header), header_cmp)
           && std::equal(std::begin(jpeg_header4_2), std::end(jpeg_header4_2), std::begin(header) + std::size(jpeg_header4_1) + 2, header_cmp));
}

bool is_gif(Image::Header & header)
{
    const std::array<unsigned char, 6> gif_header1 = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61}; //GIF87a
    const std::array<unsigned char, 6> gif_header2 = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61}; //GIF89a

    return std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), header_cmp)
        || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), header_cmp);
}

bool is_pnm(Image::Header & header)
{
    const std::array<unsigned char, 2> pbm_header   {0x50, 0x31};
    const std::array<unsigned char, 2> pgm_header   {0x50, 0x32};
    const std::array<unsigned char, 2> ppm_header   {0x50, 0x33};
    const std::array<unsigned char, 2> pbm_b_header {0x50, 0x34};
    const std::array<unsigned char, 2> pgm_b_header {0x50, 0x35};
    const std::array<unsigned char, 2> ppm_b_header {0x50, 0x36};

    return std::equal(std::begin(pbm_header),   std::end(pbm_header),   std::begin(header), header_cmp)
        || std::equal(std::begin(pgm_header),   std::end(pgm_header),   std::begin(header), header_cmp)
        || std::equal(std::begin(ppm_header),   std::end(ppm_header),   std::begin(header), header_cmp)
        || std::equal(std::begin(pbm_b_header), std::end(pbm_b_header), std::begin(header), header_cmp)
        || std::equal(std::begin(pgm_b_header), std::end(pgm_b_header), std::begin(header), header_cmp)
        || std::equal(std::begin(ppm_b_header), std::end(ppm_b_header), std::begin(header), header_cmp);
}

[[nodiscard]] std::unique_ptr<Image> get_image_data(std::string & input_filename, int bg)
{
    std::ifstream input_file;
    if(input_filename != "-")
        input_file.open(input_filename, std::ios_base::in | std::ios_base::binary);
    std::istream & input = (input_filename == "-") ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file: " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));
    if(input.eof()) // technically, some image files could be smaller than 12 bytes, but they wouldn't be interesting images
        throw std::runtime_error{"Could not read file header: not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file: " + std::string{std::strerror(errno)}};

    if(is_png(header))
    {
    #ifdef HAS_PNG
        return std::make_unique<Png>(header, input, bg);
    #else
        throw std::runtime_error{"Not compiled with PNG support"};
    #endif
    }
    else if(is_jpeg(header))
    {
    #ifdef HAS_JPEG
        return std::make_unique<Jpeg>(header, input);
    #else
        throw std::runtime_error{"Not compiled with JPEG support"};
    #endif
    }
    else if(is_gif(header))
    {
    #ifdef HAS_GIF
        return std::make_unique<Gif>(header, input, bg);
    #else
        throw std::runtime_error{"Not compiled with GIF support"};
    #endif
    }
    else if(is_pnm(header))
    {
        return std::make_unique<Pnm>(header, input);
    }
    else
    {
        throw std::runtime_error{"Unknown input file format\n"};
    }
}
