#include "tiff.hpp"

#include <cstring>

#include <tiff.h>
#include <tiffio.h>

struct Tiff_reader
{
    Tiff_reader(const Image::Header & header, std::istream & input):
        data(std::begin(header), std::end(header))
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


Tiff::Tiff(const Header & header, std::istream & input, unsigned char bg)
{
    // libtiff does kind of a stupid thing and will seek backwards, which Header_stream doesn't support (because we can read from a pipe)
    // read the whole, huge file into memory instead
    Tiff_reader tiff_reader(header, input);

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

    width_ = w;
    height_ = h;

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto pix = raster[row * width_ + col];
            unsigned char r = TIFFGetR(pix);
            unsigned char g = TIFFGetG(pix);
            unsigned char b = TIFFGetB(pix);
            unsigned char a = TIFFGetA(pix);

            auto val = rgb_to_gray(r, g, b) / 255.0f;
            auto alpha = a / 255.0f;
            image_data_[row][col] = static_cast<unsigned char>((val * alpha + (bg / 255.0f) * (1.0f - alpha)) * 255.0f);
        }
    }

    TIFFClose(tiff);
}
