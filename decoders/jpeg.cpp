#include "jpeg.hpp"

#include <iostream>

Jpeg::Jpeg(const Header & header, std::istream & input)
{
    jpeg_decompress_struct cinfo;
    my_jpeg_error jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_jpeg_error::exit;

    my_jpeg_source source(header, input);

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

    width_ = cinfo.output_width;
    height_ = cinfo.output_height;

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

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

void Jpeg::my_jpeg_error::exit(j_common_ptr cinfo) noexcept
{
    cinfo->err->output_message(cinfo);
    std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);
}

Jpeg::my_jpeg_source::my_jpeg_source(const Header & header, std::istream & input):
    header_{header},
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

boolean Jpeg::my_jpeg_source::my_fill_input_buffer(j_decompress_ptr cinfo) noexcept
{
    auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

    std::size_t jpeg_ind = 0;
    while(src.header_bytes_read_ < std::size(src.header_) && jpeg_ind < std::size(src.buffer_))
        src.buffer_[jpeg_ind++] = src.header_[src.header_bytes_read_++];

    src.input_.read(reinterpret_cast<char *>(std::data(src.buffer_)) + jpeg_ind, std::size(src.buffer_) - jpeg_ind);

    src.next_input_byte = std::data(src.buffer_);
    src.bytes_in_buffer = src.input_.gcount() + jpeg_ind;

    if(src.input_.bad() || src.bytes_in_buffer == 0)
    {
        std::cerr<<"ERROR: Could not read JPEG image\n";
        src.buffer_[0] = 0xFF;
        src.buffer_[1] = JPEG_EOI;
        src.bytes_in_buffer = 2;
    }

    return true;
}
void Jpeg::my_jpeg_source::my_skip_input_data(j_decompress_ptr cinfo, long num_bytes) noexcept
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
