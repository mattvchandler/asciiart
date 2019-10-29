#include "tiff.hpp"

#include <stdexcept>
#include <cstring>

#include <tiff.h>
#include <tiffio.h>

struct Tiff_reader
{
    explicit Tiff_reader(std::istream & input)
    {
        std::array<char, 4096> buffer;
        while(input)
        {
            input.read(std::data(buffer), std::size(buffer));
            if(input.bad())
                throw std::runtime_error {"Error reading TIFF file"};

            data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + input.gcount());
        }
    }

    std::vector<unsigned char> data;
    std::size_t pos {0};
};

tsize_t tiff_read(thandle_t hnd, tdata_t data, tsize_t size)
{
    auto in = reinterpret_cast<Tiff_reader*>(hnd);
    auto read_size = std::min(static_cast<size_t>(size), std::size(in->data) - in->pos);
    std::memcpy(data, std::data(in->data) + in->pos, read_size);
    in->pos += read_size;
    return read_size;
}

toff_t tiff_seek(thandle_t hnd, toff_t off, int whence)
{
    auto in = reinterpret_cast<Tiff_reader*>(hnd);
    switch(whence)
    {
        case SEEK_SET:
            in->pos = off;
            break;
        case SEEK_CUR:
            in->pos += off;
            break;
        case SEEK_END:
            in->pos = std::size(in->data) - off;
    }
    return in->pos;
}

toff_t tiff_size(thandle_t hnd)
{
    auto in = reinterpret_cast<Tiff_reader*>(hnd);
    return std::size(in->data);
}


Tiff::Tiff(std::istream & input)
{
    // libtiff does kind of a stupid thing and will seek backwards, which Header_stream doesn't support (because we can read from a pipe)
    // read the whole, huge file into memory instead
    Tiff_reader tiff_reader(input);

    TIFFSetWarningHandler(nullptr);

    auto tiff = TIFFClientOpen("TIFF", "r", &tiff_reader, tiff_read, [](auto,auto,auto){return tsize_t{0};}, tiff_seek, [](auto){return 0;}, tiff_size, [](auto,auto,auto){return 0;}, [](auto,auto,auto){});
    if(!tiff)
        throw std::runtime_error{"Error reading TIFF data"};

    uint32 w, h;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);

    std::vector<uint32> raster(w * h);

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
