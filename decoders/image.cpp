#include "image.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <cmath>
#include <cstring>

#ifdef HAS_ENDIAN
#include <endian.h>
#endif
#ifdef HAS_BYTESWAP
#include <byteswap.h>
#endif

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

#ifdef BIG_ENDIAN
readb_endian host_endian = readb_endian::BE;
#else
readb_endian host_endian = readb_endian::LE;
#endif

#ifndef HAS_BSWAP16
std::uint16_t bswap_16(std::uint16_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap16(a);
    #elif defined(_MSC_VER)
    return _byteswap_ushort(a);
    #else
    return ((a >> 8) & 0x00FF) | ((a << 8) & 0xFF00);
    #endif
}
#endif

#ifndef HAS_BSWAP32
std::uint32_t bswap_32(std::uint32_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap32(a);
    #elif defined(_MSC_VER)
    return _byteswap_ulong(a);
    #else
    return ((a >> 24) & 0x000000FF) | ((a >> 8) & 0x0000FF00) | ((a << 8) & 0x00FF0000) | ((a << 24) & 0xFF000000);
    #endif
}
#endif

#ifndef HAS_LE16TOH
std::uint16_t le16toh(std::uint16_t a)
{
#ifdef BIG_ENDIAN
    return bswap_16(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE16TOH
std::uint16_t be16toh(std::uint16_t a)
{
#ifdef BIG_ENDIAN
    return a;
#else
    return bswap_16(a);
#endif
}
#endif

#ifndef HAS_LE32TOH
std::uint32_t le32toh(std::uint32_t a)
{
#ifdef BIG_ENDIAN
    return bswap_32(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE32TOH
std::uint32_t be32toh(std::uint32_t a)
{
#ifdef BIG_ENDIAN
    return a;
#else
    return bswap_32(a);
#endif
}
#endif

void readb(std::istream & i, std::uint32_t & t, readb_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == readb_endian::LE)
            t = le32toh(t);
        else
            t = be32toh(t);
    }
}
void readb(std::istream & i, std::int32_t & t, readb_endian endian)
{
    readb(i, reinterpret_cast<std::uint32_t&>(t), endian);
}
void readb(std::istream & i, std::uint16_t & t, readb_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == readb_endian::LE)
            t = le16toh(t);
        else
            t = be16toh(t);
    }
}
void readb(std::istream & i, std::int16_t & t, readb_endian endian)
{
    readb(i, reinterpret_cast<std::uint16_t&>(t), endian);
}
void readb(std::istream & i, std::uint8_t & t)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
}
void readb(std::istream & i, std::int8_t & t)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
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
