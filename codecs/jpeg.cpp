#include "jpeg.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <csetjmp>

#include <jpeglib.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
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
    explicit my_jpeg_source(std::istream & input):
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
};

exif::Orientation get_orientation([[maybe_unused]] const jpeg_decompress_struct & cinfo)
{
#ifdef EXIF_FOUND
    auto marker = cinfo.marker_list;
    while(marker)
    {
        if(marker->marker == JPEG_APP0 + 1)
            return exif::get_orientation(marker->data, marker->data_length);

        marker = marker->next;
    }
#endif

    return exif::Orientation::r_0;
};

Jpeg::Jpeg(std::istream & input)
{
    std::vector<unsigned char> buffer;

    jpeg_decompress_struct cinfo;
    my_jpeg_error jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_jpeg_error::exit;

    my_jpeg_source source(input);

    if(setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        throw std::runtime_error{"Error reading with libjpeg"};
    }

    jpeg_create_decompress(&cinfo);

    cinfo.src = &source;

    // request APP1 for EXIF for rotation
#ifdef EXIF_FOUND
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
#endif

    jpeg_read_header(&cinfo, true);
    cinfo.out_color_space = JCS_RGB;

    // get orientation from EXIF, if it exists
    auto orientation {get_orientation(cinfo)};

    jpeg_start_decompress(&cinfo);

    set_size(cinfo.output_width, cinfo.output_height);

    if(cinfo.output_components != 3)
        throw std::runtime_error{"JPEG not converted to RGB"};

    buffer.resize(cinfo.output_width * 3);

    while(cinfo.output_scanline < cinfo.output_height)
    {
        auto ptr = std::data(buffer);
        jpeg_read_scanlines(&cinfo, &ptr, 1);
        for(std::size_t i = 0; i < cinfo.output_width; ++i)
            image_data_[cinfo.output_scanline - 1][i] = Color{buffer[i * 3], buffer[i * 3 + 1], buffer[i * 3 + 2]};
    }

    // rotate as needed
    transpose_image(orientation);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

class my_jpeg_dest: public jpeg_destination_mgr
{
public:
    explicit my_jpeg_dest(std::ostream & output):
        output_{output}
    {
        init_destination = init;
        empty_output_buffer = empty_buffer;
        term_destination = term;
        next_output_byte = nullptr;
        free_in_buffer = 0;
    }

private:

    static void init(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);
        dest.next_output_byte = std::data(dest.buffer_);
        dest.free_in_buffer = std::size(dest.buffer_);
    }

    static boolean empty_buffer(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);
        dest.output_.write(reinterpret_cast<char *>(std::data(dest.buffer_)), std::size(dest.buffer_));
        if(dest.output_.bad())
            std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);

        dest.next_output_byte = std::data(dest.buffer_);
        dest.free_in_buffer = std::size(dest.buffer_);

        return true;
    }

    static void term(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);

        if(dest.free_in_buffer < std::size(dest.buffer_))
        {
            dest.output_.write(reinterpret_cast<char *>(std::data(dest.buffer_)), std::size(dest.buffer_) - dest.free_in_buffer);
            dest.output_.flush();
        }
    }

    std::ostream & output_;
    std::array<JOCTET, 4096> buffer_;
};

void Jpeg::write(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    std::vector<unsigned char> buffer(img.get_width() * 3);

    jpeg_compress_struct cinfo;
    my_jpeg_error jerr;

    cinfo.err = jpeg_std_error(&jerr);

    my_jpeg_dest dest(out);

    if(setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_compress(&cinfo);
        throw std::runtime_error{"Error writing with libjpeg"};
    }

    jpeg_create_compress(&cinfo);

    cinfo.dest = &dest;
    cinfo.image_width = img.get_width();
    cinfo.image_height = img.get_height();
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);

    jpeg_start_compress(&cinfo, true);

    while(cinfo.next_scanline < cinfo.image_height)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            FColor fcolor = img[cinfo.next_scanline][col];
            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            buffer[col * 3    ] = static_cast<unsigned char>(fcolor.r * 255.0f);
            buffer[col * 3 + 1] = static_cast<unsigned char>(fcolor.g * 255.0f);
            buffer[col * 3 + 2] = static_cast<unsigned char>(fcolor.b * 255.0f);
        }

        auto buffer_ptr = std::data(buffer);
        jpeg_write_scanlines(&cinfo, &buffer_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}
