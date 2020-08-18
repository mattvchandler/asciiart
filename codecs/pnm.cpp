#include "pnm.hpp"

#include <algorithm>
#include <bitset>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <cstdint>

#include "binio.hpp"

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

std::uint16_t read_val(std::istream & in)
{
    try
    {
        auto val = std::stol(read_skip_comments(in));
        if(val < std::numeric_limits<std::uint16_t>::min() || val > std::numeric_limits<std::uint16_t>::max())
            throw std::out_of_range{""};

        return static_cast<std::uint16_t>(val);
    }
    catch(const std::invalid_argument&)
    {
        throw std::runtime_error{"Error reading PNM header: dimensions invalid"};
    }
    catch(const std::out_of_range&)
    {
        throw std::runtime_error{"Error reading PNM header: dimensions out of range"};
    }
}

Pnm::Pnm(std::istream & input)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    std::string type;
    try
    {
        input >> type;

        // PAM files store their height and width differently than the rest, so skip the rest of this and handle it there
        if(type == "P7")
        {
            read_P7(input);
            return;
        }

        try
        {
            auto width = std::stoull(read_skip_comments(input));
            auto height = std::stoull(read_skip_comments(input));
            set_size(width, height);
        }
        catch(const std::invalid_argument&)
        {
            throw std::runtime_error{"Error reading PNM: value invalid"};
        }
        catch(const std::out_of_range&)
        {
            throw std::runtime_error{"Error reading PNM: value out of range"};
        }
    }
    catch(std::ios_base::failure&)
    {
        throw std::runtime_error{"Error reading PNM: could not read file"};
    }

    try
    {
        switch(type[1])
        {
        case '1':
            read_P1(input);
            break;
        case '2':
            read_P2(input);
            break;
        case '3':
            read_P3(input);
            break;
        case '4':
            read_P4(input);
            break;
        case '5':
            read_P5(input);
            break;
        case '6':
            read_P6(input);
            break;
        case '7':
            read_P6(input);
            break;
        case 'F':
            read_PF_color(input);
            break;
        case 'f':
            read_PF_gray(input);
            break;
        }
    }
    catch(std::ios_base::failure&)
    {
        if(input.bad())
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
                image_data_[row][col] = Color{0xFF};
                break;
            case '1':
                image_data_[row][col] = Color{};
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
            auto v = read_val(input);

            if(v > max_val)
                throw std::runtime_error{"Error reading PGM: pixel value out of range"};

            image_data_[row][col] = Color{static_cast<unsigned char>(v / max_val * 255.0f)};
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
            auto r = read_val(input);
            auto g = read_val(input);
            auto b = read_val(input);

            if(r > max_val || g > max_val || b > max_val)
                throw std::runtime_error{"Error reading PPM: pixel value out of range"};

            image_data_[row][col] = Color{static_cast<unsigned char>(r / max_val * 255.0f), static_cast<unsigned char>(g / max_val * 255.0f), static_cast<unsigned char>(b / max_val * 255.0f)};
        }
    }
}

void Pnm::read_P4(std::istream & input)
{
    // ignore trailing space after header
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::bitset<8> bits;
    int bits_read {0};

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            if(bits_read == 0)
                bits = input.get();

            if(bits[7 - bits_read])
                image_data_[row][col] = Color{};
            else
                image_data_[row][col] = Color{0xFF};

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
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for(std::size_t row = 0; row < height_; ++row)
    {
        if(max_val <= std::numeric_limits<std::uint8_t>::max())
        {
            std::vector<unsigned char> rowbuf(width_);
            input.read(reinterpret_cast<char *>(std::data(rowbuf)), std::size(rowbuf));
            std::transform(std::begin(rowbuf), std::end(rowbuf), std::begin(image_data_[row]), [max_val](unsigned char a) { return Color{static_cast<unsigned char>(a / max_val * 255.0f)}; });
        }
        else
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                std::uint16_t val;
                readb(input, val, binio_endian::BE);
                image_data_[row][col] = Color{static_cast<unsigned char>(val / max_val * 255.0f)};
            }
        }
    }
}

void Pnm::read_P6(std::istream & input)
{
    auto max_val = static_cast<float>(read_val(input));

    // ignore trailing space after header
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for(std::size_t row = 0; row < height_; ++row)
    {
        if(max_val <= std::numeric_limits<std::uint8_t>::max())
        {
            std::vector<unsigned char> rowbuf(width_ * 3);
            input.read(reinterpret_cast<char *>(std::data(rowbuf)), std::size(rowbuf));
            for(std::size_t col = 0; col < width_; ++col)
            {
                image_data_[row][col].r = static_cast<unsigned char>(rowbuf[3 * col]     / max_val * 255.0f);
                image_data_[row][col].g = static_cast<unsigned char>(rowbuf[3 * col + 1] / max_val * 255.0f);
                image_data_[row][col].b = static_cast<unsigned char>(rowbuf[3 * col + 2] / max_val * 255.0f);
            }
        }
        else
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                std::uint16_t r, g, b;
                readb(input, r, binio_endian::BE);
                readb(input, g, binio_endian::BE);
                readb(input, b, binio_endian::BE);
                image_data_[row][col] = Color{static_cast<unsigned char>(r / max_val * 255.0f),
                                              static_cast<unsigned char>(g / max_val * 255.0f),
                                              static_cast<unsigned char>(b / max_val * 255.0f)};
            }
        }
    }
}

template <typename T>
auto pam_read_pix(std::istream & input, std::size_t depth, float max_val_f)
{
    Color c;
    switch(depth)
    {
        case 1:
        {
            T v;
            readb(input, v);
            c = Color{static_cast<unsigned char>(static_cast<float>(v) / max_val_f * 255.0f)};
            break;
        }
        case 2:
        {
            T v, a;
            readb(input, v);
            readb(input, a);
            auto v_i = static_cast<unsigned char>(static_cast<float>(v) / max_val_f * 255.0f);
            c = Color{v_i, v_i, v_i, static_cast<unsigned char>(static_cast<float>(a) / max_val_f * 255.0f)};
            break;
        }
        case 3:
        {
            T r, g, b;
            readb(input, r);
            readb(input, g);
            readb(input, b);
            c = Color{static_cast<unsigned char>(static_cast<float>(r) / max_val_f * 255.0f),
                      static_cast<unsigned char>(static_cast<float>(g) / max_val_f * 255.0f),
                      static_cast<unsigned char>(static_cast<float>(b) / max_val_f * 255.0f)};
            break;
        }
        case 4:
        {
            T r, g, b, a;
            readb(input, r);
            readb(input, g);
            readb(input, b);
            readb(input, a);
            c = Color{static_cast<unsigned char>(static_cast<float>(r) / max_val_f * 255.0f),
                      static_cast<unsigned char>(static_cast<float>(g) / max_val_f * 255.0f),
                      static_cast<unsigned char>(static_cast<float>(b) / max_val_f * 255.0f),
                      static_cast<unsigned char>(static_cast<float>(a) / max_val_f * 255.0f)};
            break;
        }
    }

    return c;
}

void Pnm::read_P7(std::istream & input)
{
    std::optional<std::size_t> width, height, depth;
    std::optional<std::uint16_t> max_val;
    std::optional<std::string> tupletype;

    // ignore trailing space after header
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    while(true)
    {
        std::string line;
        std::getline(input, line);

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter(tokens));

        if(std::empty(tokens) || std::empty(tokens[0]) || tokens[0][0] == '#')
            continue;

        try
        {
            if(tokens[0] == "ENDHDR")
            {
                break;
            }
            else if(tokens[0] == "WIDTH")
            {
                auto tmp = std::stoull(tokens.at(1));
                if(tmp < 1 || tmp > std::numeric_limits<std::size_t>::max())
                    throw std::out_of_range{""};
                width.emplace(tmp);
            }
            else if(tokens[0] == "HEIGHT")
            {
                auto tmp = std::stoull(tokens.at(1));
                if(tmp < 1 || tmp > std::numeric_limits<std::size_t>::max())
                    throw std::out_of_range{""};
                height.emplace(tmp);
            }
            else if(tokens[0] == "DEPTH")
            {
                auto tmp = std::stoull(tokens.at(1));
                if(tmp < 1 || tmp > std::numeric_limits<std::size_t>::max())
                    throw std::out_of_range{""};
                depth.emplace(tmp);
            }
            else if(tokens[0] == "MAXVAL")
            {
                auto tmp = std::stoul(tokens.at(1));
                if(tmp < 1 || tmp > std::numeric_limits<std::uint16_t>::max())
                    throw std::out_of_range{""};
                max_val.emplace(tmp);
            }
            else if(tokens[0] == "TUPLTYPE")
            {
                tupletype.emplace(line.substr(std::size(tokens[0]) + 1));
            }
        }
        catch(const std::invalid_argument&)
        {
            throw std::runtime_error{"Invalid PAM header: " + line};
        }
        catch(const std::out_of_range&)
        {
            throw std::runtime_error{"Invalid PAM header: " + line};
        }
    }

    if(!width)
        throw std::runtime_error{"PAM missing required WIDTH header"};
    if(!height)
        throw std::runtime_error{"PAM missing required HEIGHT header"};
    if(!depth)
        throw std::runtime_error{"PAM missing required DEPTH header"};
    if(!max_val)
        throw std::runtime_error{"PAM missing required MAXVAL header"};

    set_size(*width, *height);

    if(!(*depth == 1 && (*tupletype == "BLACKANDWHITE" || *tupletype == "GRAYSCALE")) &&
       !(*depth == 2 && (*tupletype == "BLACKANDWHITE_ALPHA" || *tupletype == "GRAYSCALE_ALPHA")) &&
       !(*depth == 3 && *tupletype == "RGB") &&
       !(*depth == 4 && *tupletype == "RGB_ALPHA"))
    {
        throw std::runtime_error{"Unsupported PAM format"};
    }

    float max_val_f = static_cast<float>(*max_val);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            if(max_val > std::numeric_limits<std::uint8_t>::max())
                image_data_[row][col] = pam_read_pix<std::uint16_t>(input, *depth, max_val_f);
            else
                image_data_[row][col] = pam_read_pix<std::uint8_t>(input, *depth, max_val_f);
        }
    }
}

auto read_pf_header(std::istream & input)
{
    auto max_val = std::stof(read_skip_comments(input));
    auto endian = max_val >= 0.0f ? binio_endian::BE : binio_endian::LE;
    max_val = std::abs(max_val);

    // ignore trailing space after header
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    return std::pair{max_val, endian};
}

void Pnm::read_PF_color(std::istream & input)
{
    auto [max_val, endian] = read_pf_header(input);

    for(std::size_t row = height_; row -- > 0;) // PFM is bottom-to-top
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            float r, g, b;
            readb(input, r, endian);
            readb(input, g, endian);
            readb(input, b, endian);
            image_data_[row][col] = Color{static_cast<unsigned char>(r / max_val * 255.0f),
                                          static_cast<unsigned char>(g / max_val * 255.0f),
                                          static_cast<unsigned char>(b / max_val * 255.0f)};
        }
    }
}

void Pnm::read_PF_gray(std::istream & input)
{
    auto [max_val, endian] = read_pf_header(input);

    for(std::size_t row = height_; row -- > 0;) // PFM is bottom-to-top
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            float val;
            readb(input, val, endian);
            image_data_[row][col] = Color { static_cast<unsigned char>(val / max_val * 255.0f) };
        }
    }
}

void Pnm::write_pbm(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    out<<"P4\n"<<img.get_width()<<" "<<img.get_height()<<'\n';

    Image img_copy(img.get_width(), img.get_height());
    for(std::size_t row = 0; row < img_copy.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img_copy.get_width(); ++col)
        {
            FColor fcolor {img[row][col]};

            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            auto l = static_cast<unsigned char>(fcolor.to_gray() * 255.0f);
            img_copy[row][col] = {l, l, l, 255};
        }
    }

    std::vector bw_palette {Color{0}, Color{255}};
    img_copy.dither(std::begin(bw_palette), std::end(bw_palette));

    std::bitset<8> bits {0};
    int bits_written {0};

    auto write = [&out, &bits, &bits_written]()
    {
        out.put(static_cast<unsigned char>(bits.to_ulong()));
        bits = 0;
        bits_written = 0;
    };

    for(std::size_t row = 0; row < img_copy.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img_copy.get_width(); ++col)
        {
            bits[7 - (bits_written++)] = img_copy[row][col].r == 0;

            if(bits_written >= 8)
                write();
        }
        if(bits_written > 0)
            write();
    }
}

void Pnm::write_pgm(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    out<<"P5\n"<<img.get_width()<<" "<<img.get_height()<<"\n255\n";

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            FColor fcolor {img[row][col]};

            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            out.put(static_cast<unsigned char>(fcolor.to_gray() * 255.0f));
        }
    }
}

void Pnm::write_ppm(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    out<<"P6\n"<<img.get_width()<<" "<<img.get_height()<<"\n255\n";

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            FColor fcolor {img[row][col]};

            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            Color color = fcolor;

            out.put(color.r);
            out.put(color.g);
            out.put(color.b);
        }
    }
}
