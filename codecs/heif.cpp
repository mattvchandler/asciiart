#include "heif.hpp"

#include <libheif/heif_cxx.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

Heif::Heif(std::istream & input)
{
    auto data = Image::read_input_to_memory(input);

    heif::Context context;
    try
    {
        context.read_from_memory_without_copy(std::data(data), std::size(data));
        auto handle = context.get_primary_image_handle();

        auto orientation { exif::Orientation::r_0};
        #ifdef EXIF_FOUND
        auto metadata_ids = handle.get_list_of_metadata_block_IDs("Exif");
        for(auto && id: metadata_ids)
        {
            auto metadata = handle.get_metadata(id);
            // libheif documentation says to skip the first 4 bytes of exif metadata
            orientation = exif::get_orientation(std::data(metadata) + 4, std::size(metadata) - 4);
        }
        #endif // EXIF_FOUND

        auto image = handle.decode_image(heif_colorspace_RGB, heif_chroma_interleaved_RGBA);

        set_size(image.get_width(heif_channel_interleaved), image.get_height(heif_channel_interleaved));

        int row_stride = 0;
        auto plane = image.get_plane(heif_channel_interleaved, &row_stride);
        if(!plane)
            throw std::runtime_error{"Error reading HEIF image data"};

        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                if(image.get_chroma_format() == heif_chroma_interleaved_RGBA)
                {
                    image_data_[row][col] = Color{plane[row * row_stride + col * 4],
                                                  plane[row * row_stride + col * 4 + 1],
                                                  plane[row * row_stride + col * 4 + 2],
                                                  plane[row * row_stride + col * 4 + 3]};
                }
                else if(image.get_chroma_format() == heif_chroma_interleaved_RGB)
                {
                    image_data_[row][col] = Color{plane[row * row_stride +  col * 3],
                                                  plane[row * row_stride +  col * 3 + 1],
                                                  plane[row * row_stride +  col * 3 + 2]};
                }
                else if(image.get_chroma_format() == heif_chroma_monochrome)
                {
                    image_data_[row][col] = Color{plane[row * row_stride +  col]};
                }
            }
        }

        // rotate as needed
        transpose_image(orientation);
    }
    catch(const heif::Error & e)
    {
        throw std::runtime_error{"Error reading HEIF file: " + e.get_message()};
    }
}

void Heif::write(std::ostream & out, const Image & img, bool invert)
{
    try
    {
        heif::Image image;
        image.create(img.get_width(), img.get_height(), heif_colorspace_RGB, heif_chroma_interleaved_RGBA);
        image.add_plane(heif_channel_interleaved, img.get_width(), img.get_height(), 8);

        int row_stride = 0;
        auto plane = image.get_plane(heif_channel_interleaved, &row_stride);
        if(!plane)
            throw std::runtime_error{"Error writing HEIF image data"};

        for(std::size_t row = 0; row < img.get_height(); ++row)
        {
            for(std::size_t col = 0; col < img.get_width(); ++col)
            {
                for(std::size_t i = 0; i < 4; ++i)
                {
                    if(invert && i < 3)
                        plane[row * row_stride + col * 4 + i] = 255 - img[row][col][i];
                    else
                        plane[row * row_stride + col * 4 + i] = img[row][col][i];
                }
            }
        }

        heif::Context context;
        heif::Encoder encoder(heif_compression_HEVC);
        context.encode_image(image, encoder);

        class Ostream_writer: public heif::Context::Writer
        {
        public:
            explicit Ostream_writer(std::ostream & out): out_{out} {};
            heif_error write(const void* data, size_t size) override
            {
                out_.write(static_cast<const char *>(data), size);
                return out_ ? heif_error{heif_error_Ok, heif_suberror_Unspecified, ""} : heif_error{heif_error_Encoding_error, heif_suberror_Cannot_write_output_data, "Output error"};
            }
        private:
            std::ostream & out_;
        } writer{out};

        context.write(writer);
    }
    catch(const heif::Error & e)
    {
        throw std::runtime_error{"Error writing HEIF file: " + e.get_message()};
    }
}
