#include "webp.hpp"

#include <webp/decode.h>

Webp::Webp(const Header & header, std::istream & input, unsigned char bg)
{
    // read whole into memory
    std::vector<unsigned char> data(std::begin(header), std::end(header));
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

    width_ = width;
    height_ = height;

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

    uint8_t * pix_data = WebPDecodeRGBA(reinterpret_cast<uint8_t *>(std::data(data)), std::size(data), &width, &height);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto r = pix_data[4 * (row * width_ + col)];
            auto g = pix_data[4 * (row * width_ + col) + 1];
            auto b = pix_data[4 * (row * width_ + col) + 2];
            auto a = pix_data[4 * (row * width_ + col) + 3];

            auto val = rgb_to_gray(r, g, b) / 255.0f;
            auto alpha = a / 255.0f;
            image_data_[row][col] = static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
        }
    }

    WebPFree(pix_data);
}
