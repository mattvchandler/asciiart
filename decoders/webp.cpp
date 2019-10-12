#include "webp.hpp"

#include <webp/decode.h>

Webp::Webp(std::istream & input, unsigned char bg)
{
    // read whole into memory
    std::vector<unsigned char> data;
    std::array<char, 4096> buffer;
    while(input)
    {
        input.read(std::data(buffer), std::size(buffer));
        if(input.bad())
            throw std::runtime_error {"Error reading WEBP file"};

        data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + input.gcount());
    }

    int width, height;
    if(!WebPGetInfo(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height))
        throw std::runtime_error{"Invalid WEBP header\n"};

    set_size(width, height);

    uint8_t * pix_data = WebPDecodeRGBA(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto r = pix_data[4 * (row * width_ + col)];
            auto g = pix_data[4 * (row * width_ + col) + 1];
            auto b = pix_data[4 * (row * width_ + col) + 2];
            auto a = pix_data[4 * (row * width_ + col) + 3];

            image_data_[row][col] = rgba_to_gray(r, g, b, a, bg);
        }
    }

    WebPFree(pix_data);
}
