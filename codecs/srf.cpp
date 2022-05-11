#include "srf.hpp"

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <cstdint>

#include "binio.hpp"

// Garmin GPS Vehicle icon
// Format documented at http://www.techmods.net/nuvi/

struct SRF_image
{
    std::uint16_t height {0};
    std::uint16_t width {0};
    std::vector<std::uint8_t> alpha_mask;
    std::vector<std::uint16_t> image;
};

SRF_image read_image_data(std::istream & input)
{
    // header
    input.ignore(12); // unknown

    SRF_image im;

    readb(input, im.height);
    readb(input, im.width);

    input.ignore(2); // unknown

    std::uint16_t rowstride;
    readb(input, rowstride);
    if(rowstride != im.width * 2)
        throw std::runtime_error{"SRF rowstride mismatched " + std::to_string(rowstride) + " vs " + std::to_string(im.width * 2)};

    input.ignore(4); // unknown

    // Alpha Mask
    input.ignore(4); // unknown

    std::uint32_t alpha_mask_size;
    readb(input, alpha_mask_size);

    if(alpha_mask_size != im.width * im.height)
        throw std::runtime_error{"SRF alpha size mismatched " + std::to_string(alpha_mask_size) + " vs " + std::to_string(im.width * im.height)};

    im.alpha_mask.resize(im.width * im.height);
    input.read(reinterpret_cast<char *>(std::data(im.alpha_mask)), std::size(im.alpha_mask) * sizeof(decltype(im.alpha_mask)::value_type));

    // image data (16-bit)
    input.ignore(4); // unknown

    std::uint32_t image_size;
    readb(input, image_size);
    if(image_size != im.width * im.height * 2)
        throw std::runtime_error{"SRF image size mismatched " + std::to_string(image_size) + " vs " + std::to_string(im.width * im.height * 2)};

    im.image.resize(im.width * im.height);
    if(image_size)
        input.read(reinterpret_cast<char *>(std::data(im.image)), std::size(im.image) * sizeof(decltype(im.image)::value_type));

    return im;
}

Color get_image_color(std::size_t row,
                      std::size_t col,
                      const SRF_image & im)
{
    auto alpha = static_cast<std::uint8_t>(std::lround((static_cast<float>(128 - im.alpha_mask[row * im.width + col])) * 255.0f / 128.0f)); // 0-128, awkward range

    auto raw_color = im.image[row * im.width + col];
    auto r = static_cast<std::uint8_t>((raw_color & 0xF800) >> 11) << 3;
    auto g = static_cast<std::uint8_t>((raw_color & 0x07C0) >>  6) << 3;
    auto b = static_cast<std::uint8_t>((raw_color & 0x003F) >>  0) << 3;
    return Color(r, g, b, alpha);
}

void Srf::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        // header
        input.ignore(16); // magic
        input.ignore(8); // unknown

        std::uint32_t num_images;
        readb(input, num_images);

        input.ignore(4);
        std::uint32_t garmin_strlen;
        readb(input, garmin_strlen);
        input.ignore(garmin_strlen); // 578 string

        std::uint32_t unknown_switch;
        readb(input, unknown_switch);
        if(unknown_switch != 6)
            throw std::runtime_error{"Unsupported SRF image format"};

        readb(input, garmin_strlen);
        input.ignore(garmin_strlen); // version no
        input.ignore(4); // unknown
        readb(input, garmin_strlen);
        input.ignore(garmin_strlen); // product code?

        std::vector<SRF_image> images;

        std::uint16_t max_width = 0;
        std::uint16_t total_height = 0;

        for(std::uint32_t i = 0; i < num_images; ++i)
        {
            images.emplace_back(read_image_data(input));
            max_width = std::max(max_width, images.back().width);
            total_height += images.back().height;
        }

        set_size(max_width, total_height);

        std::size_t current_row = 0;
        for(auto &im: images)
        {
            for(std::size_t row = 0; row < im.height; ++row, ++current_row)
            {
                for(std::size_t col = 0; col < im.width; ++col)
                {
                    image_data_[current_row][col] = get_image_color(row, col, im);
                }
            }
        }
    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading SRF: could not read file"};
        else
            throw std::runtime_error{"Error reading SRF: unexpected end of file"};
    }
}
