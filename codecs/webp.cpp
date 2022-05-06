#include "webp.hpp"

#include <stdexcept>

#include <webp/decode.h>
#include <webp/encode.h>

Webp::Webp(std::istream & input, const Args & args)
{
    handle_extra_args(args);
    auto data = Image::read_input_to_memory(input);

    int width, height;
    if(!WebPGetInfo(reinterpret_cast<std::uint8_t *>(std::data(data)), std::size(data), &width, &height))
        throw std::runtime_error{"Invalid WEBP header\n"};

    set_size(width, height);

    uint8_t * pix_data = WebPDecodeRGBA(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            for(std::size_t i = 0; i < 4; ++i)
            {
                image_data_[row][col][i] = pix_data[4 * (row * width_ + col) + i];
            }
        }
    }

    WebPFree(pix_data);
}

void Webp::write(std::ostream & out, const Image & img, bool invert)
{
    std::vector<std::uint8_t> data(img.get_width() * img.get_height() * 4);

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            for(std::size_t i = 0; i < 4; ++i)
            {
                if(invert && i < 3)
                    data[row * img.get_width() * 4 + col * 4 + i] = 255 - img[row][col][i];
                else
                    data[row * img.get_width() * 4 + col * 4 + i] = img[row][col][i];
            }
        }
    }

    std::uint8_t * output;

    auto output_size = WebPEncodeLosslessRGBA(std::data(data), img.get_width(), img.get_width(), img.get_width() * 4, &output);

    out.write(reinterpret_cast<char *>(output), output_size);

    WebPFree(output);
}
