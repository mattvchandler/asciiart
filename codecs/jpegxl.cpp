#include "jpegxl.hpp"

#include <iostream>
#include <stdexcept>
#include <cstdint>

#include <jpegxl/decode.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

struct Decoder_wrapper
{
    Decoder_wrapper(): decoder { JpegxlDecoderCreate(nullptr) }
    {
        if(!decoder)
            throw std::runtime_error {"Could not created JPEG XL decoder"};
    }
    ~Decoder_wrapper() { JpegxlDecoderDestroy(decoder); }
    operator JpegxlDecoder*() { return decoder; };
    operator const JpegxlDecoder*() const { return decoder; };

    JpegxlDecoder * decoder = nullptr;
};

JpegXL::JpegXL(std::istream & input)
{
    std::cerr<<"Warning: JPEG XL support is experimental. Success will vary depending on the image and jpeg xl library version\n";

    Decoder_wrapper decoder;

    JpegxlPixelFormat format
    {
        4,                    // num_channels
        JPEGXL_LITTLE_ENDIAN, // endianness
        JPEGXL_TYPE_UINT8     // data_type
    };

    auto data = Image::read_input_to_memory(input);

    const uint8_t * next_data = std::data(data);
    std::size_t avail_data = std::size(data);

    if(JpegxlDecoderSubscribeEvents(decoder, JPEGXL_DEC_BASIC_INFO | JPEGXL_DEC_FULL_IMAGE) != JPEGXL_DEC_SUCCESS)
        throw std::runtime_error {"Error subscibing to JPEG XL events"};

    if(JpegxlDecoderProcessInput(decoder, &next_data, &avail_data) != JPEGXL_DEC_BASIC_INFO)
        throw std::runtime_error {"Error decoding JPEG XL header. Invalid data"};

    std::size_t buffer_size {0};
    if(JpegxlDecoderImageOutBufferSize(decoder, &format, &buffer_size) != JPEGXL_DEC_SUCCESS)
        throw std::runtime_error {"Error getting JPEG XL decompressed size"};

    JpegxlBasicInfo info;
    if(JpegxlDecoderGetBasicInfo(decoder, &info) != JPEGXL_DEC_SUCCESS)
        throw std::runtime_error {"Error decoding JPEG XL data. Unable to read JPEG XL basic info"};

    std::vector<std::uint8_t> buffer(buffer_size);

    if(JpegxlDecoderSetImageOutBuffer(decoder, &format, std::data(buffer), std::size(buffer)) != JPEGXL_DEC_SUCCESS)
        throw std::runtime_error {"Error setting JPEG XL buffer"};

    if(JpegxlDecoderProcessInput(decoder, &next_data, &avail_data) != JPEGXL_DEC_FULL_IMAGE)
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
