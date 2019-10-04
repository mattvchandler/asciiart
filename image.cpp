#include "image.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

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
    if(input.eof())
        throw std::runtime_error{"Could not read file header: not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file: " + std::string{std::strerror(errno)}};

    auto header_cmp = [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); };

    const std::array<unsigned char, 8>  png_header     = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const std::array<unsigned char, 4>  jpeg_header1   = {0XFF, 0xD8, 0xFF, 0xDB};
    const std::array<unsigned char, 12> jpeg_header2   = {0XFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01};
    const std::array<unsigned char, 4>  jpeg_header3   = {0XFF, 0xD8, 0xFF, 0xEE};
    const std::array<unsigned char, 4>  jpeg_header4_1 = {0XFF, 0xD8, 0xFF, 0xE1};
    const std::array<unsigned char, 6>  jpeg_header4_2 = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00}; // there are 2 "don't care" bytes between these
    const std::array<unsigned char, 6>  gif_header1    = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};
    const std::array<unsigned char, 6>  gif_header2    = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};

    if(std::equal(std::begin(png_header), std::end(png_header), std::begin(header), header_cmp))
    {
    #ifdef HAS_PNG
        return std::make_unique<Png>(header, input, bg);
    #else
        throw std::runtime_error{"Not compiled with PNG support"};
    #endif
    }
    else if(std::equal(std::begin(jpeg_header1),   std::end(jpeg_header1),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header2),   std::end(jpeg_header2),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
        ||  std::equal(std::begin(jpeg_header3),   std::end(jpeg_header3),   std::begin(header), header_cmp)
        || (std::equal(std::begin(jpeg_header4_1), std::end(jpeg_header4_1), std::begin(header), header_cmp)
            && std::equal(std::begin(jpeg_header4_2), std::end(jpeg_header4_2), std::begin(header) + std::size(jpeg_header4_1) + 2, header_cmp)))
    {
    #ifdef HAS_JPEG
        return std::make_unique<Jpeg>(header, input);
    #else
        throw std::runtime_error{"Not compiled with JPEG support"};
    #endif
    }
    else if(std::equal(std::begin(gif_header1), std::end(gif_header1), std::begin(header), header_cmp)
         || std::equal(std::begin(gif_header2), std::end(gif_header2), std::begin(header), header_cmp))
    {
    #ifdef HAS_GIF
        return std::make_unique<Gif>(header, input, bg);
    #else
        throw std::runtime_error{"Not compiled with GIF support"};
    #endif
    }
    else
    {
        throw std::runtime_error{"Unknown input file format\n"};
    }
}
