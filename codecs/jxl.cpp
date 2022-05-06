#include "jxl.hpp"

#include <iostream>
#include <stdexcept>

#include <cstdint>

#include <jxl/decode_cxx.h>
#include <jxl/encode_cxx.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Jxl::Jxl(std::istream & input, const Args & args)
{
    handle_extra_args(args);
    std::cerr<<"Warning: JPEG XL input support is experimental. Success will vary depending on the image and jpeg xl library version\n";

    auto decoder = JxlDecoderMake(nullptr);
    if(!decoder)
        throw std::runtime_error {"Could not create JPEG XL decoder"};

    JxlPixelFormat format
    {
        4,                 // num_channels
        JXL_TYPE_UINT8,    // data_type
        JXL_LITTLE_ENDIAN, // endianness
        0                  // align
    };

    auto data = Image::read_input_to_memory(input);

    if(JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error subscibing to JPEG XL events"};

    if(JxlDecoderSetInput(decoder.get(), std::data(data), std::size(data)) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error supplying JPEG XL input data"};

    if(JxlDecoderProcessInput(decoder.get()) != JXL_DEC_BASIC_INFO)
        throw std::runtime_error {"Error decoding JPEG XL header. Invalid data"};

    std::size_t buffer_size {0};
    if(JxlDecoderImageOutBufferSize(decoder.get(), &format, &buffer_size) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error getting JPEG XL decompressed size"};

    JxlBasicInfo info{};
    if(JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error decoding JPEG XL data. Unable to read JPEG XL basic info"};

    std::vector<std::uint8_t> buffer(buffer_size);

    if(JxlDecoderSetImageOutBuffer(decoder.get(), &format, std::data(buffer), std::size(buffer)) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error setting JPEG XL buffer"};

    if(JxlDecoderProcessInput(decoder.get()) != JXL_DEC_FULL_IMAGE)
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

void Jxl::write(std::ostream & out, const Image & img, bool invert)
{
    std::cerr<<"Warning: JPEG XL output support is experimental. Success will vary depending on the image and jpeg xl library version\n";

    auto encoder = JxlEncoderMake(nullptr);
    if(!encoder)
        throw std::runtime_error {"Could not create JPEG XL encoder"};

    JxlPixelFormat format
    {
        4,                 // num_channels
        JXL_TYPE_UINT8,    // data_type
        JXL_LITTLE_ENDIAN, // endianness
        0                  // align
    };
    JxlBasicInfo info {};
    info.xsize = static_cast<std::uint32_t>(img.get_width());
    info.ysize = static_cast<std::uint32_t>(img.get_height());
    info.num_color_channels = 3;
    info.num_extra_channels = 1;
    info.bits_per_sample = 8;
    info.alpha_bits = 8;

    std::vector<std::uint8_t> data(img.get_width() * img.get_height() * 4);

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            for(std::size_t i = 0; i < 4; ++i)
            {
                if(invert && i < 3)
                    data[(row * img.get_width() + col) * 4 + i] = 255 - img[row][col][i];
                else
                    data[(row * img.get_width() + col) * 4 + i] = img[row][col][i];
            }
        }
    }

    auto encoder_opts = JxlEncoderOptionsCreate(encoder.get(), nullptr);
    if(!encoder_opts)
        throw std::runtime_error {"Could not create JPEG XL encoder options"};

    if(JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not set JPEG XL basic info"};

    JxlColorEncoding color;
    JxlColorEncodingSetToSRGB(&color, false);
    if(JxlEncoderSetColorEncoding(encoder.get(), &color) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not set JPEG XL color encoding"};

    if(JxlEncoderAddImageFrame(encoder_opts, &format, std::data(data), std::size(data) * sizeof(decltype(data)::value_type)) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not add JPEG XL image data"};

    JxlEncoderCloseInput(encoder.get());

    std::vector<std::uint8_t> output_buffer(1024);
    std::size_t output_offset = 0;

    while(true)
    {
        std::uint8_t * next_out = std::data(output_buffer) + output_offset;
        std::size_t avail_out = std::size(output_buffer) - output_offset;

        auto status = JxlEncoderProcessOutput(encoder.get(), &next_out, &avail_out);
        if(status == JXL_ENC_SUCCESS)
        {
            output_buffer.resize(next_out - std::data(output_buffer));
            break;
        }
        else if(status == JXL_ENC_NEED_MORE_OUTPUT)
        {
            output_offset = next_out - std::data(output_buffer);
            output_buffer.resize(std::size(output_buffer) * 2);
        }
        else
        {
            throw std::runtime_error {"Could not encode JPEG XL data"};
        }
    }

    out.write(reinterpret_cast<const char *>(std::data(output_buffer)), std::size(output_buffer));
}
