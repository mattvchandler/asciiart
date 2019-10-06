#include "pnm.hpp"

#include <algorithm>
#include <bitset>

std::string read_skip_comments(std::istream & in)
{
    std::string str;
    in >> str;
    while(str[0] == '#')
    {
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        in >> str;
    }
    return str;
}

unsigned char read_val(std::istream & in)
{
    int val = 0;
    try
    {
        val = std::stoi(read_skip_comments(in));
        if(val > 255 || val < 0)
            throw std::out_of_range{""};

        return static_cast<unsigned char>(val);
    }
    catch(const std::invalid_argument & e)
    {
        throw std::runtime_error{"Error reading PNM header: dimensions invalid"};
    }
    catch(const std::out_of_range & e)
    {
        throw std::runtime_error{"Error reading PNM header: dimensions out of range"};
    }
}

Pnm::Pnm(const Header & header, std::istream & input)
{
    Header_stream header_stream{header, input};;

    std::string type;
    try
    {
        header_stream >> type;

        try
        {
            width_ = std::stoull(read_skip_comments(header_stream));
            height_ = std::stoull(read_skip_comments(header_stream));
        }
        catch(const std::invalid_argument & e)
        {
            throw std::runtime_error{"Error reading PNM header: dimensions invalid"};
        }
        catch(const std::out_of_range & e)
        {
            throw std::runtime_error{"Error reading PNM header: dimensions out of range"};
        }
    }
    catch(std::ios_base::failure & e)
    {
        throw std::runtime_error{"Error reading PNM header: could not read file"};
    }

    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);

    try
    {
        switch(type[1])
        {
        case '1':
            read_P1(header_stream);
            break;
        case '2':
            read_P2(header_stream);
            break;
        case '3':
            read_P3(header_stream);
            break;
        case '4':
            read_P4(header_stream);
            break;
        case '5':
            read_P5(header_stream);
            break;
        case '6':
            read_P6(header_stream);
            break;
        }
    }
    catch(std::ios_base::failure & e)
    {
        if(header_stream.bad())
            throw std::runtime_error{"Error reading PNM: could not read file"};
        else
            throw std::runtime_error{"Error reading PNM: unexpected end of file"};
    }
}

void Pnm::read_P1(std::istream & input)
{
    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            int v = 0;
            do { v = input.get(); } while(std::isspace(v));

            switch(v)
            {
            case '0':
                image_data_[row][col] = 0;
                break;
            case '1':
                image_data_[row][col] = 255;
                break;
            default:
                throw std::runtime_error{"Error reading PBM: unknown character: " + std::string{(char)v}};
            }
        }
    }
}

void Pnm::read_P2(std::istream & input)
{
    auto max_val = static_cast<float>(read_val(input));

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            unsigned char v = read_val(input);

            if(v > max_val)
                throw std::runtime_error{"Error reading PGM: pixel value out of range"};

            image_data_[row][col] = v / max_val * 255.0f;
        }
    }
}

void Pnm::read_P3(std::istream & input)
{
    auto max_val = static_cast<float>(read_val(input));

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            unsigned char r = read_val(input);
            unsigned char g = read_val(input);
            unsigned char b = read_val(input);

            if(r > max_val || g > max_val || b > max_val)
                throw std::runtime_error{"Error reading PPM: pixel value out of range"};

            image_data_[row][col] = rgb_to_gray(r / max_val, g / max_val, b / max_val);
        }
    }
}

void Pnm::read_P4(std::istream & input)
{
    // ignore trailing space after header
    input.ignore(1);
    std::bitset<8> bits;
    int bits_read = 0;
    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            if(bits_read == 0)
                bits = input.get();

            if(bits[7 - bits_read])
                image_data_[row][col] = 255;
            else
                image_data_[row][col] = 0;

            if(++bits_read >= 8)
                bits_read = 0;
        }
        bits_read = 0;
    }
}

void Pnm::read_P5(std::istream & input)
{
    auto max_val = static_cast<float>(read_val(input));
    // ignore trailing space after header
    input.ignore(1);

    for(std::size_t row = 0; row < height_; ++row)
    {
        std::vector<char> rowbuf(width_);
        input.read(std::data(rowbuf), std::size(rowbuf));
        std::transform(std::begin(rowbuf), std::end(rowbuf), std::begin(image_data_[row]), [max_val](char a) { return static_cast<unsigned char>(a) / max_val * 255.0f; });
    }
}

void Pnm::read_P6(std::istream & input)
{
    auto max_val = static_cast<float>(read_val(input));
    // ignore trailing space after header
    input.ignore(1);
    for(std::size_t row = 0; row < height_; ++row)
    {
        std::vector<char> rowbuf(width_ * 3);
        input.read(std::data(rowbuf), std::size(rowbuf));
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto r = static_cast<unsigned char>(rowbuf[3 * col]);
            auto g = static_cast<unsigned char>(rowbuf[3 * col + 1]);
            auto b = static_cast<unsigned char>(rowbuf[3 * col + 2]);
            image_data_[row][col] = rgb_to_gray(r / max_val, g / max_val, b / max_val);
        }
    }
}
