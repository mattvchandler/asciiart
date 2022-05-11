#include "openexr.hpp"

#include <algorithm>

#include <cstring>

#include <ImfIO.h>
#include <ImfRgbaFile.h>
#include <stdlib.h>

class OpenExr_reader: public Imf::IStream
{
public:

    OpenExr_reader(std::istream & input):
        Imf::IStream("memory"),
        data_{Image::read_input_to_memory(input)},
        pos_{std::data(data_)}
    {}

    virtual bool isMemoryMapped() const override
    {
        return true;
    }

    virtual char * readMemoryMapped(int n) override
    {
        if(std::distance(std::data(data_), pos_ + n) > static_cast<std::ptrdiff_t>(std::size(data_)))
            throw std::runtime_error{"Attempted to read past end of EXR file"};

        auto ret = pos_;
        pos_ += n;
        return reinterpret_cast<char *>(ret);
    }

    virtual bool read(char c[], int n) override
    {
        if(std::distance(std::data(data_), pos_ + n) > static_cast<std::ptrdiff_t>(std::size(data_)))
            throw std::runtime_error{"Attempted to read past end of EXR file"};

        std::memcpy(c, pos_, n);
        pos_ += n;
        return true;
    }

    virtual Imf::Int64 tellg() override
    {
        return std::distance(std::data(data_), pos_);
    }

    virtual void seekg(Imf::Int64 pos) override
    {
        if(std::distance(std::data(data_), std::data(data_) + pos) > static_cast<std::ptrdiff_t>(std::size(data_)))
            throw std::runtime_error{"Attempted to read past end of EXR file"};

        pos_ = std::data(data_) + pos;
    }

private:
    std::vector<unsigned char> data_;
    unsigned char * pos_;
};

class OpenExr_writer: public Imf::OStream
{
public:

    OpenExr_writer():
        Imf::OStream("memory")
    {}

    virtual void write(const char c[], int n) override
    {
        if(pos_ + n > std::size(data_))
            data_.resize(pos_ + n);

        std::copy(c, c + n, std::begin(data_) + pos_);
        pos_ += n;
    }

    virtual Imf::Int64 tellp() override
    {
        return pos_;
    }

    virtual void seekp(Imf::Int64 pos) override
    {
        pos_ = pos;
        if(pos_ > std::size(data_))
            data_.resize(pos_);
    }

    void output(std::ostream & out)
    {
        out.write(std::data(data_), std::size(data_));
    }

private:
    std::vector<char> data_;
    std::size_t pos_ {0};
};

OpenEXR::OpenEXR(std::istream & input)
{
    try
    {
        OpenExr_reader reader{input};
        Imf::RgbaInputFile file{reader};

        const auto dimensions = file.dataWindow();
        set_size(dimensions.max.x - dimensions.min.x + 1, dimensions.max.y - dimensions.min.y + 1);

        std::vector<Imf::Rgba> rowbuf(width_);
        for(std::size_t row = 0; row < height_; ++row)
        {
            file.setFrameBuffer(std::data(rowbuf) - dimensions.min.x - (dimensions.min.y + row) * width_, 1, width_); // no idea why the API will have the base pointer begfore the start of data
            file.readPixels(row + dimensions.min.y);

            for(std::size_t col = 0; col < width_; ++col)
            {
                image_data_[row][col].r = rowbuf[col].r * 255.0f;
                image_data_[row][col].g = rowbuf[col].g * 255.0f;
                image_data_[row][col].b = rowbuf[col].b * 255.0f;
                image_data_[row][col].a = rowbuf[col].a * 255.0f;
            }
        }
    }
    catch(Iex::BaseExc & e)
    {
        throw std::runtime_error{"Error reading EXR file: " + std::string{e.what()}};
    }
}

void OpenEXR::write(std::ostream & out, const Image & img, bool invert)
{
    try
    {
        OpenExr_writer writer;
        Imf::RgbaOutputFile file{writer, Imf::Header{static_cast<int>(img.get_width()), static_cast<int>(img.get_height())}, Imf::WRITE_RGBA};

        std::vector<Imf::Rgba> rowbuf(img.get_width());
        for(std::size_t row = 0; row < img.get_height(); ++row)
        {
            file.setFrameBuffer(std::data(rowbuf) - row * img.get_width(), 1, img.get_width());
            file.writePixels(1);

            for(std::size_t col = 0; col < img.get_width(); ++col)
            {
                if(invert)
                {
                    rowbuf[col].r = img[row][col].r / 255.0f;
                    rowbuf[col].g = img[row][col].g / 255.0f;
                    rowbuf[col].b = img[row][col].b / 255.0f;
                }
                else
                {
                    rowbuf[col].r = 1.0f - img[row][col].r / 255.0f;
                    rowbuf[col].g = 1.0f - img[row][col].g / 255.0f;
                    rowbuf[col].b = 1.0f - img[row][col].b / 255.0f;
                }
                rowbuf[col].a = img[row][col].a / 255.0f;
            }
        }

        writer.output(out);
    }
    catch(Iex::BaseExc & e)
    {
        throw std::runtime_error{"Error reading EXR file: " + std::string{e.what()}};
    }
}
