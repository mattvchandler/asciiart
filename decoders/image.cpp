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

#ifdef HAS_XPM
#include "xpm.hpp"
#endif

#include "bmp.hpp"
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

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

Header_stream::Header_stream(const Image::Header & header, std::istream & input):
    std::istream{&buf_}, buf_{header, input}
{
}

Header_stream::Header_buf::Header_buf(const Image::Header & header, std::istream & input): input_{input}
{
    static_assert(buffer_size >= std::size(Image::Header()));

    std::copy(std::begin(header), std::end(header), std::begin(buffer_));
    input_.read(std::data(buffer_) + std::size(header), std::size(buffer_) - std::size(header));

    auto buffer_size = input_.gcount() + std::size(header);
    pos_ = buffer_size;
    setg(std::data(buffer_), std::data(buffer_), std::data(buffer_) + buffer_size);
}

int Header_stream::Header_buf::underflow()
{
    if(gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    input_.read(std::data(buffer_), std::size(buffer_));
    if(input_.bad())
        return traits_type::eof();

    auto buffer_size = input_.gcount();
    pos_ += buffer_size;

    if(buffer_size == 0)
        return traits_type::eof();

    setg(std::data(buffer_), std::data(buffer_), std::data(buffer_) + buffer_size);

    return traits_type::to_int_type(*gptr());
}

Header_stream::pos_type Header_stream::Header_buf::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
{
    if(off == 0 && dir == std::ios_base::cur && which == std::ios_base::in)
        return pos_ - off_type{egptr() - gptr()};
    else
        return off_type{-1};
}

template<typename T> void readb(std::istream & i, T& t)
{
    i.read(reinterpret_cast<char *>(&t), sizeof(T));
}

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args)
{
    std::ifstream input_file;
    if(args.input_filename != "-")
        input_file.open(args.input_filename, std::ios_base::in | std::ios_base::binary);
    std::istream & input = (args.input_filename == "-") ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file: " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));
    if(input.eof()) // technically, some image files could be smaller than 12 bytes, but they wouldn't be interesting images
        throw std::runtime_error{"Could not read file header: not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file: " + std::string{std::strerror(errno)}};

    switch(args.force_file)
    {
    case Args::Force_file::detect:
        if(is_png(header))
        {
            #ifdef HAS_PNG
            return std::make_unique<Png>(header, input, args.bg);
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
            return std::make_unique<Gif>(header, input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with GIF support"};
            #endif
        }
        else if(is_bmp(header))
        {
            return std::make_unique<Bmp>(header, input, args.bg);
        }
        else if(is_pnm(header))
        {
            return std::make_unique<Pnm>(header, input);
        }
        else
        {
            throw std::runtime_error{"Unknown input file format\n"};
        }
        break;
    #ifdef HAS_XPM
    case Args::Force_file::xpm:
        return std::make_unique<Xpm>(header, input, args.bg);
        break;
    #endif
    }
}
