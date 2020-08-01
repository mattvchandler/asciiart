#include "avif.hpp"

#include "avif/avif.h"

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Avif::Avif(std::istream & input)
{
    auto data = Image::read_input_to_memory(input);

    avifROData raw;
    raw.data = std::data(data);
    raw.size = std::size(data);

    avifDecoder * decoder = avifDecoderCreate();
    if(!decoder)
        throw std::runtime_error{"Error creating AVIF decoder"};

    try
    {
        if(auto result = avifDecoderParse(decoder, &raw); result != AVIF_RESULT_OK)
            throw std::runtime_error{"Error reading AVIF file: " + std::string{avifResultToString(result)}};

        if(decoder->imageCount < 1)
            throw std::runtime_error{"Error reading AVIF: no image found"};

        if(auto result = avifDecoderNextImage(decoder); result != AVIF_RESULT_OK)
            throw std::runtime_error{"Error reading AVIF 1st frame: " + std::string{avifResultToString(result)}};

        auto orientation { exif::Orientation::r_0};
        #ifdef EXIF_FOUND
        // TODO: this is totally unteseted. I have been unable to find or create any AVIF files with EXIF data
        if(decoder->image->exif.size > 0)
            orientation = exif::get_orientation(decoder->image->exif.data, decoder->image->exif.size);
        #endif // EXIF_FOUND

        set_size(decoder->image->width, decoder->image->height);

        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, decoder->image);
        rgb.format = AVIF_RGB_FORMAT_RGBA;
        rgb.depth = 8;

        avifRGBImageAllocatePixels(&rgb);
        avifImageYUVToRGB(decoder->image, &rgb);

        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                auto pixel = rgb.pixels + row * rgb.rowBytes + col * 4;
                image_data_[row][col] = Color{ pixel[0], pixel[1], pixel[2], pixel[3] };
            }
        }

        avifRGBImageFreePixels(&rgb);

        // rotate as needed
        transpose_image(orientation);
    }
    catch(...)
    {
        avifDecoderDestroy(decoder);
        throw;
    }

    avifDecoderDestroy(decoder);
}
