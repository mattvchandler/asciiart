#include "jpeg.hpp"

#include <iostream>

#include <csetjmp>

#include <jpeglib.h>

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

    jpeg_read_header(&cinfo, true);
    cinfo.out_color_space = JCS_GRAYSCALE;

    jpeg_start_decompress(&cinfo);

    set_size(cinfo.output_width, cinfo.output_height);

    if(cinfo.output_width * cinfo.output_components != width_)
        throw std::runtime_error{"JPEG bytes per row incorrect"};

    while(cinfo.output_scanline < cinfo.output_height)
    {
        auto buffer = std::data(image_data_[cinfo.output_scanline]);
        jpeg_read_scanlines(&cinfo, &buffer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}
