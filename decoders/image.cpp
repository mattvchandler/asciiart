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

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

void Image::set_size(std::size_t w, std::size_t h)
{
    width_ = w; height_ = h;
    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);
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
    std::istream & input = args.input_filename == "-" ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));

    if(input.eof()) // technically, some image files could be smaller than 12 bytes, but they wouldn't be interesting images
        throw std::runtime_error{"Could not read file header " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

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
            return std::make_unique<Bmp>(input);
        }
        else if(is_gif(header))
        {
            #ifdef GIF_FOUND
            return std::make_unique<Gif>(input);
            #else
            throw std::runtime_error{"Not compiled with GIF support"};
            #endif
        }
        else if(is_jpeg(header))
        {
            #ifdef JPEG_FOUND
            return std::make_unique<Jpeg>(input);
            #else
            throw std::runtime_error{"Not compiled with JPEG support"};
            #endif
        }
        else if(is_png(header))
        {
            #ifdef PNG_FOUND
            return std::make_unique<Png>(input);
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
            #ifdef TIFF_FOUND
            return std::make_unique<Tiff>(input);
            #else
            throw std::runtime_error{"Not compiled with TIFF support"};
            #endif
        }
        else if(is_webp(header))
        {
            #ifdef WEBP_FOUND
            return std::make_unique<Webp>(input);
            #else
            throw std::runtime_error{"Not compiled with WEBP support"};
            #endif
        }
        else if(extension == ".svg" || extension == ".svgz")
        {
            #ifdef SVG_FOUND
            return std::make_unique<Svg>(input, args.input_filename);
            #else
            throw std::runtime_error{"Not compiled with SVG support"};
            #endif
        }
        else if(extension == ".tga")
        {
            return std::make_unique<Tga>(input);
        }
        else if(extension == ".xpm")
        {
            #ifdef XPM_FOUND
            return std::make_unique<Xpm>(input);
            #else
            throw std::runtime_error{"Not compiled with XPM support"};
            #endif
        }
        else
        {
            throw std::runtime_error{"Unknown input file format"};
        }
        break;
    #ifdef SVG_FOUND
    case Args::Force_file::svg:
        return std::make_unique<Svg>(input, args.input_filename);
        break;
    #endif
    case Args::Force_file::tga:
        return std::make_unique<Tga>(input);
        break;
    #ifdef XPM_FOUND
    case Args::Force_file::xpm:
        return std::make_unique<Xpm>(input);
        break;
    #endif
    default:
        throw std::runtime_error{"Unhandled file format switch"};
    }
}
