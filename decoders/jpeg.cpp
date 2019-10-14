#include "jpeg.hpp"

#include <algorithm>
#include <iostream>

#include <csetjmp>

#include <jpeglib.h>

#ifdef HAS_EXIF
#include <libexif/exif-data.h>
#endif

struct my_jpeg_error: public jpeg_error_mgr
{
    jmp_buf setjmp_buffer;
    static void exit(j_common_ptr cinfo) noexcept
    {
        cinfo->err->output_message(cinfo);
        std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);
    }
};

class my_jpeg_source: public jpeg_source_mgr
{
public:
    my_jpeg_source(std::istream & input):
        input_{input}
    {
        init_source = [](j_decompress_ptr){};
        fill_input_buffer = my_fill_input_buffer;
        skip_input_data = my_skip_input_data;
        resync_to_restart = jpeg_resync_to_restart;
        term_source = [](j_decompress_ptr){};
        bytes_in_buffer = 0;
        next_input_byte = nullptr;
    }

private:
    static boolean my_fill_input_buffer(j_decompress_ptr cinfo) noexcept
    {
        auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

        src.input_.read(reinterpret_cast<char *>(std::data(src.buffer_)), std::size(src.buffer_));

        src.next_input_byte = std::data(src.buffer_);
        src.bytes_in_buffer = src.input_.gcount();

        if(src.input_.bad() || src.bytes_in_buffer == 0)
        {
            std::cerr<<"ERROR: Could not read JPEG image\n";
            src.buffer_[0] = 0xFF;
            src.buffer_[1] = JPEG_EOI;
            src.bytes_in_buffer = 2;
        }

        return true;
    }

    static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes) noexcept
    {
        if(num_bytes > 0)
        {
            auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

            while(src.bytes_in_buffer < static_cast<decltype(src.bytes_in_buffer)>(num_bytes))
            {
                num_bytes -= src.bytes_in_buffer;
                my_fill_input_buffer(cinfo);
            }

            src.next_input_byte += num_bytes;
            src.bytes_in_buffer -= num_bytes;
        }
    }

    std::istream & input_;
    std::array<JOCTET, 4096> buffer_;
    JOCTET * buffer_p_ { std::data(buffer_) };
};

enum class Orientation:short { r_0=1, r_180=3, r_270=6, r_90=8 };
Orientation get_orientation([[maybe_unused]] jpeg_decompress_struct & cinfo)
{
    Orientation orientation {Orientation::r_0};
#ifdef HAS_EXIF
    auto marker = cinfo.marker_list;
    while(marker)
    {
        if(marker->marker == JPEG_APP0 + 1)
        {
            auto data = exif_data_new_from_data(marker->data, marker->data_length);
            if(data)
            {
                auto orientation_entry = exif_data_get_entry(data, EXIF_TAG_ORIENTATION);
                if(orientation_entry && orientation_entry->format == EXIF_FORMAT_SHORT)
                {
                    orientation = static_cast<Orientation>(exif_get_short(orientation_entry->data, exif_data_get_byte_order(data)));
                    if(orientation != Orientation::r_0 && orientation != Orientation::r_180 && orientation != Orientation::r_270 && orientation != Orientation::r_90)
                    {
                        std::vector<char> desc(256);
                        exif_entry_get_value(orientation_entry, std::data(desc), std::size(desc));
                        throw std::runtime_error{"Unsupported JPEG rotation: " + std::string{std::data(desc)} + " (" + std::to_string(static_cast<std::underlying_type_t<Orientation>>(orientation)) + ")"};
                    }
                }
                exif_data_unref(data);
            }
        }
        marker = marker->next;
    }
#endif

    return orientation;
};

Jpeg::Jpeg(std::istream & input)
{
    jpeg_decompress_struct cinfo;
    my_jpeg_error jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_jpeg_error::exit;

    my_jpeg_source source(input);

    jpeg_create_decompress(&cinfo);

    if(setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        throw std::runtime_error{"Error reading with libjpg"};
    }

    cinfo.src = &source;

    // request APP1 for EXIF for rotation
#ifdef HAS_EXIF
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
#endif

    jpeg_read_header(&cinfo, true);
    cinfo.out_color_space = JCS_GRAYSCALE;

    // get orientation from EXIF, if it exists
    auto orientation {get_orientation(cinfo)};

    jpeg_start_decompress(&cinfo);

    // prepare a buffer for transposed data if rotated 90 or 270 degrees
    std::vector<std::vector<unsigned char>> transpose_buf;
    auto * output_buf = &image_data_;
    if(orientation == Orientation::r_90 || orientation == Orientation::r_270)
    {
        set_size(cinfo.output_height, cinfo.output_width);

        transpose_buf.resize(cinfo.output_height);
        for(auto & row: transpose_buf)
            row.resize(cinfo.output_width);

        output_buf = &transpose_buf;
    }
    else
    {
        set_size(cinfo.output_width, cinfo.output_height);
    }

    if(cinfo.output_components != 1)
        throw std::runtime_error{"JPEG not converted to grayscale"};

    while(cinfo.output_scanline < cinfo.output_height)
    {
        auto buffer = std::data((*output_buf)[cinfo.output_scanline]);
        jpeg_read_scanlines(&cinfo, &buffer, 1);
    }

    // rotate as needed
    switch(orientation)
    {
    case Orientation::r_0:
        break;
    case Orientation::r_180: // reverse rows and columns in-place
        std::reverse(std::begin(image_data_), std::end(image_data_));
        for(auto && row: image_data_)
            std::reverse(std::begin(row), std::end(row));
        break;
    case Orientation::r_270:
        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                image_data_[row][col] = transpose_buf[width_ - col - 1][row];
            }
        }
        break;
    case Orientation::r_90:
        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                image_data_[row][col] = transpose_buf[col][height_ - row - 1];
            }
        }
        break;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}
