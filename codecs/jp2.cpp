#include "jp2.hpp"

#include <array>
#include <iostream>

#include <cstring>

#include <openjpeg.h>

#ifdef EXIF_FOUND
#include <libexif/exif-data.h>
#endif

#include "jp2_color.hpp"

struct JP2_io
{
    std::size_t pos {0};
    std::vector<unsigned char> data;
};

OPJ_SIZE_T read_fun(void * buffer, OPJ_SIZE_T num_bytes, void * user)
{
    auto input = reinterpret_cast<JP2_io*>(user);
    if(!input)
        return -1;

    if(input->pos == std::size(input->data))
        return -1;

    auto remaining = std::size(input->data) - input->pos;
    if(num_bytes > remaining)
        num_bytes = remaining;

    memcpy(buffer, &input->data[input->pos], num_bytes);
    input->pos += num_bytes;

    return num_bytes;
}

OPJ_SIZE_T write_fun(void * buffer, OPJ_SIZE_T num_bytes, void * user)
{
    auto out = reinterpret_cast<JP2_io*>(user);
    if(!out)
        return -1;

    if(out->pos + num_bytes > std::size(out->data))
        out->data.resize(out->pos + num_bytes);

    std::memcpy(std::data(out->data) + out->pos, buffer, num_bytes);

    out->pos += num_bytes;

    return num_bytes;
}

OPJ_OFF_T skip_fun(OPJ_OFF_T off, void * user)
{
    auto out = reinterpret_cast<JP2_io*>(user);
    if(!out)
        return -1;

    if(off > 0 && out->pos + off > std::size(out->data))
    {
        out->data.resize(out->pos + off);
        std::memset(std::data(out->data) + out->pos, 0, off);
    }

    out->pos += off;

    return off;
}

OPJ_BOOL seek_fun(OPJ_OFF_T pos, void * user)
{
    auto out = reinterpret_cast<JP2_io*>(user);
    if(!out)
        return OPJ_FALSE;

    if(static_cast<std::size_t>(pos) > std::size(out->data))
    {
        out->data.resize(pos);
        std::memset(std::data(out->data) + out->pos, 0, pos - out->pos);
    }

    out->pos = pos;

    return OPJ_TRUE;
}

void error_cb(const char * msg, void*)
{
    std::cerr<<"[ERROR]: "<<msg<<'\n';
}

void warn_cb(const char * msg, void*)
{
    std::cerr<<"[WARNING]: "<<msg<<'\n';
}

Jp2::Jp2(std::istream & input, Type type)
{
    JP2_io reader {0, Image::read_input_to_memory(input)};

    auto codec_type {OPJ_CODEC_JP2};
    switch(type)
    {
        case Type::JP2:
            codec_type = OPJ_CODEC_JP2;
            break;
        case Type::JPX:
            codec_type = OPJ_CODEC_JPX;
            break;
        case Type::JPT:
            codec_type = OPJ_CODEC_JPT;
            break;
    }

    auto decoder = opj_create_decompress(codec_type);
    if(!decoder)
        throw std::runtime_error{"Could not create JP2 decoder"};

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    if(!opj_setup_decoder(decoder, &parameters))
    {
        opj_destroy_codec(decoder);
        throw std::runtime_error{"Could not set JP2 decoder parameters"};
    }

    auto stream = opj_stream_default_create(OPJ_STREAM_READ);
    if(!stream)
    {
        opj_destroy_codec(decoder);
        throw std::runtime_error{"Could not create JP2 stream"};
    }

    opj_stream_set_user_data(stream, &reader, nullptr);
    opj_stream_set_user_data_length(stream, std::size(reader.data));
    opj_stream_set_read_function(stream, read_fun);

    opj_set_error_handler(decoder, error_cb, nullptr);
    opj_set_warning_handler(decoder, warn_cb, nullptr);

    opj_image_t * image {nullptr};
    if(!opj_read_header(stream, decoder, &image))
    {
        opj_destroy_codec(decoder);
        opj_stream_destroy(stream);
        throw std::runtime_error{"Could not read JP2 header"};
    }

    if(!(opj_decode(decoder, stream, image) && opj_end_decompress(decoder, stream)))
    {
        opj_image_destroy(image);
        opj_destroy_codec(decoder);
        opj_stream_destroy(stream);
        throw std::runtime_error{"Could not decode JP2"};
    }
    opj_stream_destroy(stream);

    if(image->color_space == OPJ_CLRSPC_UNSPECIFIED || image->color_space == OPJ_CLRSPC_UNKNOWN)
    {
        if(image->numcomps == 1 || image->numcomps == 2)
            image->color_space = OPJ_CLRSPC_GRAY;
        else if(image->numcomps == 5)
            image->color_space = OPJ_CLRSPC_CMYK;
        else
        {
            opj_image_destroy(image);
            opj_destroy_codec(decoder);
            throw std::runtime_error{"Unknown JP2 color space"};
        }
    }

    switch(image->color_space)
    {
        case OPJ_CLRSPC_SYCC:
            if(!color_sycc_to_rgb(image))
            {
                opj_image_destroy(image);
                opj_destroy_codec(decoder);
                throw std::runtime_error{"Could not convert JP2 color space from YCbCr to RGB"};
            }
            break;
        case OPJ_CLRSPC_CMYK:
            if(!color_cmyk_to_rgb(image))
            {
                opj_image_destroy(image);
                opj_destroy_codec(decoder);
                throw std::runtime_error{"Could not convert JP2 color space from YCbCr to RGB"};
            }
            break;
        case OPJ_CLRSPC_EYCC:
            if(!color_esycc_to_rgb(image))
            {
                opj_image_destroy(image);
                opj_destroy_codec(decoder);
                throw std::runtime_error{"Could not convert JP2 color space from YCbCr to RGB"};
            }
            break;
        default:
            break;
    }

    if(image->color_space != OPJ_CLRSPC_SRGB && image->color_space != OPJ_CLRSPC_GRAY)
    {
        opj_image_destroy(image);
        opj_destroy_codec(decoder);
        throw std::runtime_error{"Unsupported JP2 color space"};
    }

    if(!image->comps[0].data)
    {
        opj_image_destroy(image);
        opj_destroy_codec(decoder);
        throw std::runtime_error{"No image data found for JP2"};
    }

    if(image->comps[0].prec != 8)
    {
        opj_image_destroy(image);
        opj_destroy_codec(decoder);
        throw std::runtime_error{"Unsupported JP2 precision: " + std::to_string(image->comps[0].prec)};
    }

    set_size(image->comps[0].w, image->comps[0].h);

    auto make_component = [&image, this](std::size_t row, std::size_t col, int component)
    {
        auto val = image->comps[component].data[row * width_ + col];
        if(image->comps[component].sgnd)
            val += 1 << (image->comps[0].prec);

        if(val < 0)
            val = 0;
        else if(val > 255)
            val = 255;

        return static_cast<unsigned char>(val);
    };

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            switch(image->numcomps)
            {
            case 1:
                image_data_[row][col] = Color{make_component(row, col, 0)};
                break;
            case 2:
                image_data_[row][col] = Color{make_component(row, col, 0),
                                              make_component(row, col, 0),
                                              make_component(row, col, 0),
                                              make_component(row, col, 1)};
                break;
            case 3:
                image_data_[row][col] = Color{make_component(row, col, 0),
                                              make_component(row, col, 1),
                                              make_component(row, col, 2)};
                break;
            case 4:
                image_data_[row][col] = Color{make_component(row, col, 0),
                                              make_component(row, col, 1),
                                              make_component(row, col, 2),
                                              make_component(row, col, 3)};
                break;
            default:
                opj_image_destroy(image);
                opj_destroy_codec(decoder);
                throw std::runtime_error{"Unsupported number of JP2 components: " + std::to_string(image->numcomps)};
            }
        }
    }

    opj_image_destroy(image);
    opj_destroy_codec(decoder);

    #ifdef EXIF_FOUND
    // this is a hack, and might not work on every image
    auto orientation { exif::Orientation::r_0};
    const std::array<unsigned char, 20> exif_start_str = {'u', 'u', 'i', 'd', 'J', 'p', 'g', 'T', 'i', 'f', 'f', 'E', 'x', 'i', 'f', '-', '>', 'J', 'P', '2'};
    const std::array<unsigned char, 6> exif_replacement_str = {'E', 'x', 'i', 'f', '\0', '\0'};

    if(auto offset = std::search(std::begin(reader.data), std::end(reader.data), std::begin(exif_start_str), std::end(exif_start_str));
            offset != std::end(reader.data))
    {
        std::vector<unsigned char> exif_buf(std::distance(offset, std::end(reader.data)) - std::size(exif_start_str) + std::size(exif_replacement_str));
        std::copy(std::begin(exif_replacement_str), std::end(exif_replacement_str), std::begin(exif_buf));
        std::copy(offset + std::size(exif_start_str), std::end(reader.data), std::begin(exif_buf) + std::size(exif_replacement_str));
        orientation = exif::get_orientation(std::data(exif_buf), std::size(exif_buf));
    }

    transpose_image(orientation);
    #endif
}

#include <iostream>

void Jp2::write(std::ostream & out, const Image & img, bool invert)
{
    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);

    std::array<opj_image_comptparm, 4> image_param {};

    for(auto && i: image_param)
    {
        i = {
            static_cast<OPJ_UINT32>(parameters.subsampling_dx), // dx
            static_cast<OPJ_UINT32>(parameters.subsampling_dy), // dy
            static_cast<OPJ_UINT32>(img.get_width()),
            static_cast<OPJ_UINT32>(img.get_height()),
            0, // x0
            0, // y0
            8, // prec
            8, // bpp
            0, // sgnd
        };
    }

    auto image = opj_image_create(4, std::data(image_param), OPJ_CLRSPC_SRGB);
    if(!image)
    {
        throw std::runtime_error{"Could not allocate JP2 image"};
    }

    image->x0 = 0;
    image->y0 = 0;
    image->x1 = (img.get_width() - 1) * parameters.subsampling_dx + 1;
    image->y1 = (img.get_height() - 1) * parameters.subsampling_dy + 1;

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            for(std::size_t i = 0; i < 4; ++i)
            {
                if(invert && i < 3)
                {
                    image->comps[i].data[row * img.get_width() + col]
                        = 255 - img[row][col][i];
                }
                else
                {
                    image->comps[i].data[row * img.get_width() + col]
                        = img[row][col][i];
                }
            }
        }
    }

    auto encoder = opj_create_compress(OPJ_CODEC_JP2);
    if(!encoder)
        throw std::runtime_error{"Could not create JP2 encoder"};

    if(!opj_setup_encoder(encoder, &parameters, image))
    {
        opj_destroy_codec(encoder);
        opj_image_destroy(image);
        throw std::runtime_error{"Could not set JP2 encoder parameters"};
    }

    auto stream = opj_stream_default_create(OPJ_STREAM_WRITE);
    if(!stream)
    {
        opj_destroy_codec(encoder);
        opj_image_destroy(image);
        throw std::runtime_error{"Could not create JP2 stream"};
    }

    JP2_io output_buffer;
    opj_stream_set_user_data(stream, &output_buffer, nullptr);
    opj_stream_set_user_data_length(stream, 0);
    opj_stream_set_write_function(stream, write_fun);
    opj_stream_set_skip_function(stream, skip_fun);
    opj_stream_set_seek_function(stream, seek_fun);

    opj_set_error_handler(encoder, error_cb, nullptr);
    opj_set_warning_handler(encoder, warn_cb, nullptr);

    if(!opj_start_compress(encoder, image, stream))
    {
        opj_destroy_codec(encoder);
        opj_stream_destroy(stream);
        opj_image_destroy(image);
    }

    if(!opj_encode(encoder, stream))
    {
        opj_destroy_codec(encoder);
        opj_stream_destroy(stream);
        opj_image_destroy(image);
    }

    opj_end_compress(encoder, stream);

    opj_destroy_codec(encoder);
    opj_stream_destroy(stream);
    opj_image_destroy(image);

    out.write(reinterpret_cast<char *>(std::data(output_buffer.data)), std::size(output_buffer.data));
}
