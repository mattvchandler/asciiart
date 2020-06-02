#include "webp.hpp"

#include <stdexcept>

#include <webp/decode.h>

Webp::Webp(std::istream & input)
{
    auto data = Image::read_input_to_memory(input);

    int width, height;
    if(!WebPGetInfo(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height))
        throw std::runtime_error{"Invalid WEBP header\n"};

    set_size(width, height);

    uint8_t * pix_data = WebPDecodeRGBA(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            image_data_[row][col].r = pix_data[4 * (row * width_ + col)];
            image_data_[row][col].g = pix_data[4 * (row * width_ + col) + 1];
            image_data_[row][col].b = pix_data[4 * (row * width_ + col) + 2];
            image_data_[row][col].a = pix_data[4 * (row * width_ + col) + 3];
        }
    }

    WebPFree(pix_data);
}
