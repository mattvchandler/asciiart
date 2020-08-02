#include "flif.hpp"

#include <flif_dec.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Flif::Flif(std::istream & input)
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
        orientation = exif::get_orientation(metadata, metadata_size);
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
