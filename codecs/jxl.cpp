#include "jxl.hpp"

#include <iostream>
#include <stdexcept>
#include <cstdint>

#include <jxl/decode_cxx.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Jxl::Jxl(std::istream & input)
{
    std::cerr<<"Warning: JPEG XL support is experimental. Success will vary depending on the image and jpeg xl library version\n";

    auto decoder = JxlDecoderMake(nullptr);
    if(!decoder)
        throw std::runtime_error {"Could not created JPEG XL decoder"};

    JxlPixelFormat format
    {
        4,                 // num_channels
        JXL_TYPE_UINT8,    // data_type
        JXL_LITTLE_ENDIAN, // endianness
        0                  // align
    };

    auto data = Image::read_input_to_memory(input);

    const uint8_t * next_data = std::data(data);
    std::size_t avail_data = std::size(data);

    if(JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error subscibing to JPEG XL events"};

    if(JxlDecoderProcessInput(decoder.get(), &next_data, &avail_data) != JXL_DEC_BASIC_INFO)
        throw std::runtime_error {"Error decoding JPEG XL header. Invalid data"};

    std::size_t buffer_size {0};
    if(JxlDecoderImageOutBufferSize(decoder.get(), &format, &buffer_size) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error getting JPEG XL decompressed size"};

    JxlBasicInfo info;
    if(JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error decoding JPEG XL data. Unable to read JPEG XL basic info"};

    std::vector<std::uint8_t> buffer(buffer_size);

    if(JxlDecoderSetImageOutBuffer(decoder.get(), &format, std::data(buffer), std::size(buffer)) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error setting JPEG XL buffer"};

    if(JxlDecoderProcessInput(decoder.get(), &next_data, &avail_data) != JXL_DEC_FULL_IMAGE)
        throw std::runtime_error {"Error decoding JPEG XL image data. Invalid data"};

    set_size(info.xsize, info.ysize);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto pixel = std::data(buffer) + (row * width_ + col) * 4;
            image_data_[row][col] = Color{ pixel[0], pixel[1], pixel[2], pixel[3] };
        }
    }
}
