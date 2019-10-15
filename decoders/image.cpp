#include "image.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <cmath>
#include <cstring>

#include "bmp.hpp"
#include "gif.hpp"
#include "jpeg.hpp"
#include "png.hpp"
#include "pnm.hpp"
#include "svg.hpp"
#include "tga.hpp"
#include "tiff.hpp"
#include "webp.hpp"
#include "xpm.hpp"

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
unsigned char rgba_to_gray(unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned char bg)
{
    auto val = rgb_to_gray(r / 255.0f, g / 255.0f, b / 255.0f);
    auto alpha = a / 255.0f;
    return static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
}
unsigned char ga_blend(unsigned char g, unsigned char a, unsigned char bg)
{
    auto val = g / 255.0f;
    auto alpha = a / 255.0f;
    return static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
}

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

void Image::set_size(std::size_t w, std::size_t h)
{
    width_ = w; height_ = h;
    image_data_.resize(height_);
    for(auto && row: image_data_)
    {
        row.resize(width_);
        std::fill(std::begin(row), std::end(row), 0);
    }
}

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args)
{
    std::string extension;
    std::ifstream input_file;
    if(args.input_filename != "-")
    {
        input_file.open(args.input_filename, std::ios_base::in | std::ios_base::binary);
        auto pos = args.input_filename.find_last_of('.');
        if(pos != std::string::npos)
            extension = args.input_filename.substr(pos);
    }
    std::istream & input = (args.input_filename == "-") ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file: " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));

    if(input.eof()) // technically, some image files could be smaller than 12 bytes, but they wouldn't be interesting images
        throw std::runtime_error{"Could not read file header: not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file: " + std::string{std::strerror(errno)}};

    // rewind (seekg(0) not always supported for pipes)
    for(auto i = std::rbegin(header); i != std::rend(header); ++i)
        input.putback(*i);
    if(input.bad())
        throw std::runtime_error{"Unable to rewind stream"};

    switch(args.force_file)
    {
    case Args::Force_file::detect:
        if(is_bmp(header))
        {
            return std::make_unique<Bmp>(input, args.bg);
        }
        else if(is_gif(header))
        {
            #ifdef HAS_GIF
            return std::make_unique<Gif>(input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with GIF support"};
            #endif
        }
        else if(is_jpeg(header))
        {
            #ifdef HAS_JPEG
            return std::make_unique<Jpeg>(input);
            #else
            throw std::runtime_error{"Not compiled with JPEG support"};
            #endif
        }
        else if(is_png(header))
        {
            #ifdef HAS_PNG
            return std::make_unique<Png>(input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with PNG support"};
            #endif
        }
        else if(is_pnm(header))
        {
            return std::make_unique<Pnm>(input);
        }
        else if(is_tiff(header))
        {
            #ifdef HAS_TIFF
            return std::make_unique<Tiff>(input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with TIFF support"};
            #endif
        }
        else if(is_webp(header))
        {
            #ifdef HAS_WEBP
            return std::make_unique<Webp>(input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with WEBP support"};
            #endif
        }
        else if(extension == ".svg" || extension == ".svgz")
        {
            #ifdef HAS_SVG
            return std::make_unique<Svg>(input, args.input_filename, args.bg);
            #else
            throw std::runtime_error{"Not compiled with SVG support"};
            #endif
        }
        else if(extension == ".tga")
        {
            return std::make_unique<Tga>(input, args.bg);
        }
        else if(extension == ".xpm")
        {
            #ifdef HAS_XPM
            return std::make_unique<Xpm>(input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with XPM support"};
            #endif
        }
        else
        {
            throw std::runtime_error{"Unknown input file format"};
        }
        break;
    #ifdef HAS_SVG
    case Args::Force_file::svg:
        return std::make_unique<Svg>(input, args.input_filename, args.bg);
        break;
    #endif
    case Args::Force_file::tga:
        return std::make_unique<Tga>(input, args.bg);
        break;
    #ifdef HAS_XPM
    case Args::Force_file::xpm:
        return std::make_unique<Xpm>(input, args.bg);
        break;
    #endif
    default:
        throw std::runtime_error{"Unhandled file format switch"};
    }
}
