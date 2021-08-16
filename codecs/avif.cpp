#include "avif.hpp"

#include "avif/avif.h"

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Avif::Avif(std::istream & input)
{
    auto data = Image::read_input_to_memory(input);

    avifDecoder * decoder = avifDecoderCreate();
    if(!decoder)
        throw std::runtime_error{"Error creating AVIF decoder"};

    try
    {
        if(auto result = avifDecoderSetIOMemory(decoder, std::data(data), std::size(data)); result != AVIF_RESULT_OK)
            throw std::runtime_error{"Error setting AVIF IO: " + std::string{avifResultToString(result)}};

        if(auto result = avifDecoderParse(decoder); result != AVIF_RESULT_OK)
            throw std::runtime_error{"Error reading AVIF file: " + std::string{avifResultToString(result)}};

        if(decoder->imageCount < 1)
            throw std::runtime_error{"Error reading AVIF: no image found"};

        if(auto result = avifDecoderNextImage(decoder); result != AVIF_RESULT_OK)
            throw std::runtime_error{"Error reading AVIF 1st frame: " + std::string{avifResultToString(result)}};

        auto orientation { exif::Orientation::r_0};
        #ifdef EXIF_FOUND
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

void Avif::write(std::ostream & out, const Image & img, bool invert)
{
    auto image = avifImageCreate(img.get_width(), img.get_height(), 8, AVIF_PIXEL_FORMAT_YUV420);

    image->colorPrimaries          = AVIF_COLOR_PRIMARIES_BT709;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    image->matrixCoefficients      = AVIF_MATRIX_COEFFICIENTS_BT709;
    image->yuvRange                = AVIF_RANGE_FULL;

    avifImageAllocatePlanes(image, AVIF_PLANES_YUV);

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);

    rgb.depth  = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;

    avifRGBImageAllocatePixels(&rgb);

    rgb.rowBytes = img.get_width() * 4;

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_height(); ++col)
        {
            auto & color = img[row][col];

            auto pixel = rgb.pixels + row * rgb.rowBytes + col * 4;
            for(std::size_t i = 0; i < 4; ++i)
            {
                if(invert && i < 4)
                    pixel[i] = 255 - color[i];
                else
                    pixel[i] = color[i];
            }
        }
    }

    avifImageRGBToYUV(image, &rgb);
    avifRGBImageFreePixels(&rgb);

    avifRWData output     = AVIF_DATA_EMPTY;
    auto encoder          = avifEncoderCreate();
    encoder->maxThreads   = 1;
    encoder->minQuantizer = AVIF_QUANTIZER_LOSSLESS;
    encoder->maxQuantizer = AVIF_QUANTIZER_LOSSLESS;

    if(auto result = avifEncoderWrite(encoder, image, &output); result != AVIF_RESULT_OK)
    {
        avifImageDestroy(image);
        avifRWDataFree(&output);
        avifEncoderDestroy(encoder);

        throw std::runtime_error{"Error writing AVIF file: " + std::string{avifResultToString(result)}};
    }

    out.write(reinterpret_cast<char *>(output.data), output.size);

    avifImageDestroy(image);
    avifRWDataFree(&output);
    avifEncoderDestroy(encoder);
}
