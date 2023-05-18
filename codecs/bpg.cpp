#include "bpg.hpp"

#include <libbpg.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

void Bpg::open(std::istream & input, const Args &)
{
    auto data = Image::read_input_to_memory(input);

    auto decoder = bpg_decoder_open();
    if(!decoder)
        throw std::runtime_error{"Could not create BPG decoder"};

    bpg_decoder_keep_extension_data(decoder, true);

    if(bpg_decoder_decode(decoder, std::data(data), std::size(data)) != 0)
    {
        bpg_decoder_close(decoder);
        throw std::runtime_error{"Could not decode BPG image"};
    }

    auto orientation { exif::Orientation::r_0};
    #ifdef EXIF_FOUND
    auto ext = bpg_decoder_get_extension_data(decoder);
    while(ext)
    {
        if(ext->tag == BPG_EXTENSION_TAG_EXIF)
        {
            std::vector<unsigned char> exif_buf(ext->buf_len + 6);
            for(auto i = 0; i < 6; ++i)
                exif_buf[i] = "Exif\0\0"[i];
            std::copy(ext->buf, ext->buf + ext->buf_len, std::begin(exif_buf) + 6);
            orientation = exif::get_orientation(std::data(exif_buf), std::size(exif_buf)).value_or(orientation);
        }
        ext = ext->next;
    }
    #endif

    BPGImageInfo info;
    if(bpg_decoder_get_info(decoder, &info) != 0)
    {
        bpg_decoder_close(decoder);
        throw std::runtime_error{"Could not get BPG info"};
    }

    set_size(info.width, info.height);

    if(bpg_decoder_start(decoder, BPG_OUTPUT_FORMAT_RGBA32) != 0)
    {
        bpg_decoder_close(decoder);
        throw std::runtime_error{"Could not convert BPG image"};
    }

    std::vector<unsigned char> row_buffer(width_ * 4);
    for(std::size_t row = 0; row < height_; ++row)
    {
        if(bpg_decoder_get_line(decoder, std::data(row_buffer)) != 0)
        {
            bpg_decoder_close(decoder);
            throw std::runtime_error{"Could not read BPG image"};
        }
        for(std::size_t col = 0; col < width_; ++col)
            image_data_[row][col] = { row_buffer[col * 4],
                                      row_buffer[col * 4 + 1],
                                      row_buffer[col * 4 + 2],
                                      row_buffer[col * 4 + 3] };
    }

    bpg_decoder_close(decoder);

    transpose_image(orientation);
}
