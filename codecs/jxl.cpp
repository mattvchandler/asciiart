#include "jxl.hpp"

#include <stdexcept>

#include <cstdint>

#include <jxl/decode_cxx.h>
#include <jxl/encode_cxx.h>

void Jxl::open(std::istream & input, const Args &)
{
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

    JxlBasicInfo info{};
    std::array<std::uint8_t, 65536> input_buffer{};
    std::size_t input_size {0};
    std::vector<std::uint8_t> buffer{};

    if(JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        throw std::runtime_error {"Error subscibing to JPEG XL events"};

    // NOTE: libjxl automatically will apply rotation if needed, so we don't have to mess about with Exif ourselves. Yay!
    // If for some reason we ever do need to get it, we add JXL_DEC_BOX to the JxlDecoderSubscribeEvents call, add JXL_DEC_BOX and JXL_DEC_BOX_NEED_MORE_OUTPUT to the switch below,
    // For JXL_DEC_BOX, we check for Exif with JxlDecoderGetBoxType and set a buffer with JxlDecoderSetBoxBuffer
    // For JXL_DEC_BOX_NEED_MORE_OUTPUT, we get the number of unused bytes in our buffer with JxlDecoderReleaseBoxBuffer, increase the buffer size, call JxlDecoderSetBoxBuffer again with an offset to where it had left off
    // After we get SUCCESS, we need to check the offset at the start of the exif buffer (32-bit big endian number, usually 0), drop that many + 4 bytes (for the offset), and then prepend "Exif\0\0" to the front. libexif will take it from there
    // JxlDecoderReleaseBoxBuffer doesn't need to be called before the decoder is destroyed - it will be automatically cleaned by the decode destructor

    for(bool decoding = true; decoding;)
    {
        auto status = JxlDecoderProcessInput(decoder.get());

        switch(status)
        {
            case JXL_DEC_SUCCESS:
            case JXL_DEC_FULL_IMAGE: // right now we're quitting becuase we don't care about anything after the full image, but if we did, we'd want to handle JXL_DEC_FULL_IMAGE separately from JXL_DEC_SUCCESS
                                     // This would come up when we're ready to handle multiple images or animations (In which case this signals the end of a single image/frame, and more remain)
                decoding = false;
                break;

            case JXL_DEC_BASIC_INFO:
                if(JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS)
                    throw std::runtime_error {"Error decoding JPEG XL data. Unable to read JPEG XL basic info"};
                break;

            case JXL_DEC_NEED_MORE_INPUT:
            {
                if(input.eof())
                    throw std::runtime_error {"Error decoding JPEG XL image: Unexpected end of input"};

                auto remaining = JxlDecoderReleaseInput(decoder.get());

                if(remaining)
                {
                    auto leftovers = std::vector(std::begin(input_buffer) + input_size - remaining, std::begin(input_buffer) + input_size);
                    std::copy(std::begin(leftovers), std::end(leftovers), std::begin(input_buffer));
                }

                input.read(reinterpret_cast<char *>(std::data(input_buffer)) + remaining, std::size(input_buffer) - remaining);
                if(input.bad())
                    throw std::runtime_error {"Error reading Jpeg XL file"};
                input_size = input.gcount() + remaining;

                if(JxlDecoderSetInput(decoder.get(), std::data(input_buffer), input_size) != JXL_DEC_SUCCESS)
                    throw std::runtime_error {"Error supplying JPEG XL input data"};

                if(input.eof())
                    JxlDecoderCloseInput(decoder.get());

                break;
            }
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
            {
                std::size_t buffer_size {0};
                if(JxlDecoderImageOutBufferSize(decoder.get(), &format, &buffer_size) != JXL_DEC_SUCCESS)
                    throw std::runtime_error {"Error getting JPEG XL decompressed size"};

                buffer.resize(buffer_size);

                if(JxlDecoderSetImageOutBuffer(decoder.get(), &format, std::data(buffer), std::size(buffer)) != JXL_DEC_SUCCESS)
                    throw std::runtime_error {"Error setting JPEG XL buffer"};

                break;
            }

            case JXL_DEC_ERROR:
                throw std::runtime_error {"Error decoding JPEG XL image"};

            default:
                throw std::runtime_error {"Unhandled status for JPEG XL ProcessInput: " + std::to_string(status)};
        }
    }

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
    JxlEncoderInitBasicInfo(&info);
    info.xsize = static_cast<std::uint32_t>(img.get_width());
    info.ysize = static_cast<std::uint32_t>(img.get_height());
    info.num_color_channels = 3;
    info.num_extra_channels = 1;
    info.bits_per_sample = 8;
    info.alpha_bits = 8;

    if(JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not set JPEG XL basic info"};

    JxlColorEncoding color;
    JxlColorEncodingSetToSRGB(&color, false);
    if(JxlEncoderSetColorEncoding(encoder.get(), &color) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not set JPEG XL color encoding"};

    auto encoder_opts = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
    if(!encoder_opts)
        throw std::runtime_error {"Could not create JPEG XL encoder options"};

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

    if(JxlEncoderAddImageFrame(encoder_opts, &format, std::data(data), std::size(data) * sizeof(decltype(data)::value_type)) != JXL_ENC_SUCCESS)
        throw std::runtime_error {"Could not add JPEG XL image data"};

    JxlEncoderCloseInput(encoder.get());

    std::array<std::uint8_t, 65536> output_buffer;

    for(bool encoding = true; encoding;)
    {
        std::uint8_t * next_out = std::data(output_buffer);
        std::size_t avail_out = std::size(output_buffer);

        auto status = JxlEncoderProcessOutput(encoder.get(), &next_out, &avail_out);
        switch(status)
        {
            case JXL_ENC_SUCCESS:
                out.write(reinterpret_cast<const char *>(std::data(output_buffer)), next_out - std::data(output_buffer));
                encoding = false;
                break;

            case JXL_ENC_NEED_MORE_OUTPUT:
                out.write(reinterpret_cast<const char *>(std::data(output_buffer)), next_out - std::data(output_buffer));
                break;

            default:
                throw std::runtime_error {"Could not encode JPEG XL data"};
        }
    }
}
