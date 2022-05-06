#include "tiff.hpp"

#include <stdexcept>
#include <cstdint>
#include <cstring>

#include <tiff.h>
#include <tiffio.h>

struct Tiff_io
{
    Tiff_io() = default;

    explicit Tiff_io(std::istream & input):
        data { Image::read_input_to_memory(input) }
    {}

    void write(std::ostream & out) const
    {
        out.write(reinterpret_cast<const char *>(std::data(data)), std::size(data));
    }

    std::vector<unsigned char> data;
    std::size_t pos {0};
};

tsize_t tiff_read(thandle_t hnd, tdata_t data, tsize_t size)
{
    auto io = reinterpret_cast<Tiff_io*>(hnd);
    auto read_size = std::min(static_cast<size_t>(size), std::size(io->data) - io->pos);
    std::memcpy(data, std::data(io->data) + io->pos, read_size);
    io->pos += read_size;
    return read_size;
}

tsize_t tiff_write(thandle_t hnd, tdata_t data, tsize_t size)
{
    auto io = reinterpret_cast<Tiff_io*>(hnd);

    if(io->pos + size > std::size(io->data))
        io->data.resize(io->pos + size);

    std::memcpy(std::data(io->data) + io->pos, data, size);
    io->pos += size;

    return size;
}

toff_t tiff_seek(thandle_t hnd, toff_t off, int whence)
{
    auto io = reinterpret_cast<Tiff_io*>(hnd);
    switch(whence)
    {
        case SEEK_SET:
            io->pos = off;
            break;
        case SEEK_CUR:
            io->pos += off;
            break;
        case SEEK_END:
            io->pos = std::size(io->data) - off;
    }
    if(io->pos > std::size(io->data))
        io->data.resize(io->pos);

    return io->pos;
}

toff_t tiff_size(thandle_t hnd)
{
    auto io = reinterpret_cast<Tiff_io*>(hnd);
    return std::size(io->data);
}

Tiff::Tiff(std::istream & input, const Args & args)
{
    handle_extra_args(args);
    // libtiff does kind of a stupid thing and will seek backwards, which Header_stream doesn't support (because we can read from a pipe)
    // read the whole, huge file into memory instead
    Tiff_io tiff_reader(input);

    TIFFSetWarningHandler(nullptr);

    auto tiff = TIFFClientOpen("TIFF", "r", &tiff_reader, tiff_read, [](auto,auto,auto){return tsize_t{0};}, tiff_seek, [](auto){return 0;}, tiff_size, [](auto,auto,auto){return 0;}, [](auto,auto,auto){});
    if(!tiff)
        throw std::runtime_error{"Error reading TIFF data"};

    std::uint32_t w, h;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);

    std::vector<std::uint32_t> raster(w * h);

    if(!TIFFReadRGBAImageOriented(tiff, w, h, std::data(raster), ORIENTATION_TOPLEFT, 0))
    {
        TIFFClose(tiff);
        throw std::runtime_error{"Error reading TIFF data"};
    }

    set_size(w, h);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto pix = raster[row * width_ + col];
            image_data_[row][col].r = TIFFGetR(pix);
            image_data_[row][col].g = TIFFGetG(pix);
            image_data_[row][col].b = TIFFGetB(pix);
            image_data_[row][col].a = TIFFGetA(pix);
        }
    }

    TIFFClose(tiff);
}

void Tiff::write(std::ostream & out, const Image & img, bool invert)
{
    Tiff_io tiff_writer;

    auto tiff = TIFFClientOpen("TIFF", "w", &tiff_writer, [](auto,auto,auto){return tsize_t{0};}, tiff_write, tiff_seek, [](auto){return 0;}, tiff_size, [](auto,auto,auto){return 0;}, [](auto,auto,auto){});
    if(!tiff)
        throw std::runtime_error{"Error writing TIFF data"};

    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, img.get_width());
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, img.get_height());
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    if(static_cast<std::size_t>(TIFFScanlineSize(tiff)) != img.get_width() * 4)
    {
        TIFFClose(tiff);
        throw std::runtime_error{"TIFF scanline size incorrect"};
    }

    std::vector<Color> rowbuf(img.get_width());
    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            rowbuf[col] = img[row][col];
            if(invert)
                rowbuf[col].invert();
        }
        if(TIFFWriteScanline(tiff, std::data(rowbuf), row, 0) < 0)
        {
            TIFFClose(tiff);
            throw std::runtime_error{"Error writing TIFF data"};
        }
    }

    TIFFClose(tiff);

    tiff_writer.write(out);
}
