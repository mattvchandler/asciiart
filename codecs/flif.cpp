#include "flif.hpp"

#ifdef FLIF_ENC_FOUND
#include <flif.h>
#elif FLIF_DEC_FOUND
#include <flif_dec.h>
#endif

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

#ifdef FLIF_DEC_FOUND
void Flif::open(std::istream & input, const Args &)
{
    auto data = Image::read_input_to_memory(input);

    auto decoder = flif_create_decoder();
    if(!decoder)
        throw std::runtime_error{"Could not create FLIF decoder"};

    if(!flif_decoder_decode_memory(decoder, std::data(data), std::size(data)))
    {
        flif_abort_decoder(decoder);
        throw std::runtime_error{"Could not decode FLIF image"};
    }

    auto image = flif_decoder_get_image(decoder, 0);
    if(!image)
    {
        flif_destroy_decoder(decoder);
        throw std::runtime_error{"Could not get FLIF image"};
    }

    auto orientation { exif::Orientation::r_0};
    #ifdef EXIF_FOUND
    unsigned char * metadata = nullptr;
    std::size_t metadata_size = 0;
    if(flif_image_get_metadata(image, "eXif", &metadata, &metadata_size))
    {
        orientation = exif::get_orientation(metadata, metadata_size).value_or(orientation);
        flif_image_free_metadata(image, metadata);
    }
    #endif

    set_size(flif_image_get_width(image), flif_image_get_height(image));

    std::vector<unsigned char> row_buffer(width_ * 4);
    for(std::size_t row = 0; row < height_; ++row)
    {
        flif_image_read_row_RGBA8(image, row, std::data(row_buffer), std::size(row_buffer));
        for(std::size_t col = 0; col < width_; ++col)
            image_data_[row][col] = { row_buffer[col * 4],
                                      row_buffer[col * 4 + 1],
                                      row_buffer[col * 4 + 2],
                                      row_buffer[col * 4 + 3] };
    }

    flif_destroy_decoder(decoder);

    transpose_image(orientation);
}
#endif

#ifdef FLIF_ENC_FOUND
void Flif::write(std::ostream & out, const Image & img, bool invert)
{
    auto encoder = flif_create_encoder();
    if(!encoder)
        throw std::runtime_error{"Could not create FLIF encoder"};

    auto image = flif_create_image(img.get_width(), img.get_height());
    if(!image)
    {
        flif_destroy_encoder(encoder);
        throw std::runtime_error{"Could not create FLIF image"};
    }

    std::vector<unsigned char> row_buffer(img.get_width() *4);
    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            for(std::size_t i = 0; i < 4; ++i)
            {
                if(i < 3 && invert)
                    row_buffer[col * 4 + i] = 255 - img[row][col][i];
                else
                    row_buffer[col * 4 + i] = img[row][col][i];
            }
        }

        flif_image_write_row_RGBA8(image, row, std::data(row_buffer), std::size(row_buffer));
    }

    flif_encoder_add_image_move(encoder, image);

    char * buffer;
    std::size_t buffer_size;

    flif_encoder_encode_memory(encoder, reinterpret_cast<void**>(&buffer), &buffer_size);
    flif_destroy_encoder(encoder);

    out.write(buffer, buffer_size);
    free(buffer);
}
#endif
