#include "image.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

#include <cassert>
#include <cmath>
#include <cstring>

#ifdef HAS_ENDIAN
#include <endian.h>
#endif
#ifdef HAS_BYTESWAP
#include <byteswap.h>
#endif

#include "avif.hpp"
#include "bmp.hpp"
#include "bpg.hpp"
#include "flif.hpp"
#include "gif.hpp"
#include "heif.hpp"
#include "ico.hpp"
#include "jp2.hpp"
#include "jpeg.hpp"
#include "png.hpp"
#include "pnm.hpp"
#include "sif.hpp"
#include "svg.hpp"
#include "tga.hpp"
#include "tiff.hpp"
#include "webp.hpp"
#include "xpm.hpp"

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

std::vector<unsigned char> Image::read_input_to_memory(std::istream & input)
{
    // read whole stream into memory
    std::vector<unsigned char> data;
    std::array<char, 4096> buffer;
    while(input)
    {
        input.read(std::data(buffer), std::size(buffer));
        if(input.bad())
            throw std::runtime_error {"Error reading input file"};

        data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + input.gcount());
    }
    return data;
}

void Image::set_size(std::size_t w, std::size_t h)
{
    width_ = w; height_ = h;
    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);
}

void Image::transpose_image(exif::Orientation orientation)
{
    if(orientation == exif::Orientation::r_90 || orientation == exif::Orientation::r_270)
    {
        // prepare a buffer for transposed data if rotated 90 or 270 degrees
        decltype(image_data_) transpose_buf;

        transpose_buf.resize(width_);
        for(auto & row: transpose_buf)
            row.resize(height_);

        for(std::size_t row = 0; row < width_; ++row)
        {
            for(std::size_t col = 0; col < height_; ++col)
            {
                if(orientation == exif::Orientation::r_90)
                    transpose_buf[row][col] = image_data_[col][width_ - row - 1];
                else // r_270
                    transpose_buf[row][col] = image_data_[height_ - col - 1][row];
            }
        }

        std::swap(width_, height_);
        std::swap(image_data_, transpose_buf);
    }
    else if(orientation == exif::Orientation::r_180)
    {
        std::reverse(std::begin(image_data_), std::end(image_data_));
        for(auto && row: image_data_)
            std::reverse(std::begin(row), std::end(row));
    }
}

Image Image::scale(std::size_t new_width, std::size_t new_height) const
{
    Image new_img;
    new_img.set_size(new_width, new_height);

    const auto px_col = static_cast<float>(width_)  / static_cast<float>(new_width);
    const auto px_row = static_cast<float>(height_) / static_cast<float>(new_height);

    float row = 0.0f;
    for(std::size_t new_row = 0; new_row < new_height; ++new_row, row += px_row)
    {
        float col = 0.0f;
        for(std::size_t new_col = 0; new_col < new_width; ++new_col, col += px_col)
        {
            float r_sum = 0.0f;
            float g_sum = 0.0f;
            float b_sum = 0.0f;
            float a_sum = 0.0f;

            float cell_count {0.0f};

            for(float y = row; y < row + px_row && y < height_; y += 1.0f)
            {
                for(float x = col; x < col + px_col && x < width_; x += 1.0f)
                {
                    auto x_ind = static_cast<std::size_t>(x);
                    auto y_ind = static_cast<std::size_t>(y);
                    if(x_ind >= width_ || y_ind >= height_)
                        throw std::runtime_error{"Output coords out of range"};

                    auto pix = image_data_[y_ind][x_ind];

                    r_sum += static_cast<float>(pix.r) * static_cast<float>(pix.r);
                    g_sum += static_cast<float>(pix.g) * static_cast<float>(pix.g);
                    b_sum += static_cast<float>(pix.b) * static_cast<float>(pix.b);
                    a_sum += static_cast<float>(pix.a) * static_cast<float>(pix.a);

                    cell_count += 1.0f;
                }
            }

            new_img.image_data_[new_row][new_col] = Color{
                static_cast<unsigned char>(std::sqrt(r_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(g_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(b_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(a_sum / cell_count))
            };
        }
    }

    return new_img;
}

std::vector<Color> Image::generate_palette(std::size_t num_colors, bool gif_transparency) const
{
    // TODO: this handles transparency extremely poorly
    if(num_colors == 0)
        throw std::domain_error {"empty palette requested"};

    if((num_colors & (num_colors - 1)) != 0)
        throw std::domain_error {"palette_size must be a power of 2"};

    std::vector<Color> palette(width_ * height_);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            palette[row * width_ + col] = image_data_[row][col];
        }
    }

    if(std::size(palette) <= num_colors)
    {
        palette.resize(num_colors);
        return palette;
    }

    std::vector partitions = { std::pair{std::begin(palette), std::end(palette)} };

    // TODO: slow
    while(std::size(partitions) < num_colors)
    {
        decltype(partitions) next_partitions;
        for(auto && [begin, end]: partitions)
        {
            // find which color has the maximum range, and sort this partion by that color's value
            Color min = {
                std::numeric_limits<decltype(Color::r)>::max(),
                std::numeric_limits<decltype(Color::g)>::max(),
                std::numeric_limits<decltype(Color::b)>::max(),
                0};
            Color max = {
                std::numeric_limits<decltype(Color::r)>::min(),
                std::numeric_limits<decltype(Color::g)>::min(),
                std::numeric_limits<decltype(Color::b)>::min(),
                0};

            for(auto i = begin; i != end; ++i)
            {
                min.r = std::min(min.r, i->r);
                max.r = std::max(max.r, i->r);
                min.g = std::min(min.g, i->g);
                max.g = std::max(max.g, i->g);
                min.b = std::min(min.b, i->b);
                max.b = std::max(max.b, i->b);
            }

            auto r_range = max.r - min.r;
            auto g_range = max.g - min.g;
            auto b_range = max.b - min.b;

            if(r_range > g_range  && r_range > b_range)
                std::sort(begin, end, [](const Color & a, const Color & b){ return a.r < b.r; });

            else if(g_range > b_range)
                std::sort(begin, end, [](const Color & a, const Color & b){ return a.g < b.g; });

            else
                std::sort(begin, end, [](const Color & a, const Color & b){ return a.b < b.b; });

            auto mid = begin + std::distance(begin, end) / 2;
            next_partitions.emplace_back(begin, mid);
            next_partitions.emplace_back(mid, end);
        }

        std::swap(next_partitions, partitions);
    }

    decltype(palette) reduced_pallete(num_colors);

    assert(std::size(partitions) == num_colors && std::size(reduced_pallete) == num_colors);

    for(std::size_t i = 0; i < num_colors; ++i)
    {
        auto && [begin, end] = partitions[i];
        std::size_t r_avg = 0, g_avg = 0, b_avg = 0, a_avg = 0;
        for(auto j = begin; j != end; ++j)
        {
            r_avg += j->r;
            g_avg += j->g;
            b_avg += j->b;
            a_avg += j->a;
        }
        auto n = std::distance(begin, end);
        reduced_pallete[i] = Color
        {
            static_cast<decltype(Color::r)>(r_avg / n),
            static_cast<decltype(Color::g)>(g_avg / n),
            static_cast<decltype(Color::b)>(b_avg / n),
            static_cast<decltype(Color::a)>(a_avg / n)
        };

        if(gif_transparency)
        {
            if(reduced_pallete[i].a > 127)
                reduced_pallete[i].a = 255;
            else
                reduced_pallete[i] = Color {0, 0, 0, 0};
        }
    }

    std::set unique_palette(std::begin(reduced_pallete), std::end(reduced_pallete));
    std::vector<Color> final_palette(num_colors);
    std::copy(std::begin(unique_palette), std::end(unique_palette), std::begin(final_palette));
    std::fill(std::begin(final_palette) + std::size(unique_palette), std::end(final_palette), Color{0});

    assert(std::size(final_palette) == num_colors);

    return final_palette;
}

void Image::convert(const Args & args) const
{
    if(!args.convert_filename)
        return;

    std::ofstream out{args.convert_filename->first, std::ios_base::binary};
    if(!out)
        throw std::runtime_error {"Could not open " + args.convert_filename->first + " for writing: " + std::strerror(errno)};

    auto & ext = args.convert_filename->second;

    if(ext == ".bmp")
        Bmp::write(out, *this, args.invert);
    #ifdef GIF_FOUND
    else if(ext == ".gif")
        Gif::write(out, *this, args.invert);
    #endif
    #ifdef JPEG_FOUND
    else if(ext == ".jpeg" || ext == ".jpg")
        Jpeg::write(out, *this, args.bg, args.invert);
    #endif
    #ifdef PNG_FOUND
    else if(ext == ".png")
        Png::write(out, *this, args.invert);
    #endif
    else if(ext == ".pbm")
        Pnm::write_pbm(out, *this, args.bg, args.invert);
    else if(ext == ".pgm")
        Pnm::write_pgm(out, *this, args.bg, args.invert);
    else if(ext == ".ppm")
        Pnm::write_ppm(out, *this, args.bg, args.invert);
    else
        throw std::runtime_error {"Unsupported conversion type: " + ext};
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
        for(auto && i: extension)
            i = std::tolower(i);
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
        if(is_avif(header))
        {
            #ifdef AVIF_FOUND
            return std::make_unique<Avif>(input);
            #else
            throw std::runtime_error{"Not compiled with AVIF support"};
            #endif
        }
        else if(is_bmp(header))
        {
            return std::make_unique<Bmp>(input);
        }
        else if(is_bpg(header))
        {
            #ifdef BPG_FOUND
            return std::make_unique<Bpg>(input);
            #else
            throw std::runtime_error{"Not compiled with BPG support"};
            #endif
        }
        else if(is_flif(header))
        {
            #ifdef FLIF_FOUND
            return std::make_unique<Flif>(input);
            #else
            throw std::runtime_error{"Not compiled with FLIF support"};
            #endif
        }
        else if(is_gif(header))
        {
            #ifdef GIF_FOUND
            return std::make_unique<Gif>(input);
            #else
            throw std::runtime_error{"Not compiled with GIF support"};
            #endif
        }
        else if(is_heif(header))
        {
            #ifdef HEIF_FOUND
            return std::make_unique<Heif>(input);
            #else
            throw std::runtime_error{"Not compiled with HEIF support"};
            #endif
        }
        else if(is_ico(header))
        {
            return std::make_unique<Ico>(input);
        }
        else if(is_jp2(header))
        {
            #ifdef JP2_FOUND
            return std::make_unique<Jp2>(input, Jp2::Type::JP2);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
            #endif
        }
        else if(is_jpx(header))
        {
            #ifdef JP2_FOUND
            return std::make_unique<Jp2>(input, Jp2::Type::JPX);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
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
        else if(extension == ".jpt")
        {
            #ifdef JP2_FOUND
            return std::make_unique<Jp2>(input, Jp2::Type::JPT);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
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
    case Args::Force_file::aoc_2019_sif:
        return std::make_unique<Sif>(input);
        break;
    default:
        throw std::runtime_error{"Unhandled file format switch"};
    }
}
