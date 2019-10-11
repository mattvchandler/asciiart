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
#include "tiff.hpp"
#include "tga.hpp"
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

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

Header_stream::Header_stream(const Image::Header & header, std::istream & input):
    std::istream{&buf_}, buf_{header, input}
{}

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
    if(which != std::ios_base::in)
        return off_type{-1};

    // shortcut for tellg
    if(off == 0 && dir == std::ios_base::cur)
        return current_pos();

    switch(dir)
    {
        case std::ios_base::beg:
            return seekpos(off, which);
        case std::ios_base::end:
            return off_type{-1}; // we can't tell where the stream ends
            break;
        case std::ios_base::cur:
            return seekpos(current_pos() + off, which);
            break;
        default:
            return off_type{-1};
            break;
    }

    return off_type{-1};
}

Header_stream::pos_type Header_stream::Header_buf::seekpos(pos_type pos, std::ios_base::openmode which)
{
    if(which != std::ios_base::in)
        return off_type{-1};

    auto current = current_pos();

    auto buff_start = pos_ - off_type{buffer_size};
    auto buff_end = pos_;
    auto offset = pos - current;

    if(pos < buff_start)
    {
        // can't go back any further
        return off_type{-1};
    }
    else if(pos >= buff_start && pos < buff_end)
    {
        // within current buffer
        setg(eback(), gptr() + offset, egptr());
    }
    else if(pos >= buff_end)
    {
        // read until we're at pos
        while(pos >= pos_)
        {
            setg(eback(), egptr(), egptr());
            underflow();
        }

        setg(eback(), gptr() + (pos - current_pos()), egptr());
    }

    return current_pos();
}

Header_stream::pos_type Header_stream::Header_buf::current_pos() const
{
    return pos_ - off_type{egptr() - gptr()};
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
        else if(is_webp(header))
        {
            #ifdef HAS_WEBP
            return std::make_unique<Webp>(header, input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with WEBP support"};
            #endif
        }
        else if(is_tiff(header))
        {
            #ifdef HAS_TIFF
            return std::make_unique<Tiff>(header, input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with TIFF support"};
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
        else if(extension == ".tga")
        {
            return std::make_unique<Tga>(header, input, args.bg);
        }
        else if(extension == ".xpm")
        {
            #ifdef HAS_XPM
            return std::make_unique<Xpm>(header, input, args.bg);
            #else
            throw std::runtime_error{"Not compiled with XPM support"};
            #endif
        }
        else
        {
            throw std::runtime_error{"Unknown input file format"};
        }
        break;
    case Args::Force_file::tga:
        return std::make_unique<Tga>(header, input, args.bg);
        break;
    #ifdef HAS_XPM
    case Args::Force_file::xpm:
        return std::make_unique<Xpm>(header, input, args.bg);
        break;
    #endif
    default:
        throw std::runtime_error{"Unhandled file format switch"};
    }
}
