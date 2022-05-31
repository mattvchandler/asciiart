#include "pkmn.hpp"

#include <iterator>

#include "bitstream.hpp"
#include "sub_args.hpp"

#include <iostream>

constexpr auto tile_size = 8u;
constexpr auto fixed_tile_size = 7u;

void decompress(Input_bitstream<std::istreambuf_iterator<char>> & bits, std::uint8_t tile_width, std::uint8_t tile_height, std::uint8_t * decompression_buffer, bool fixed_buffer)
{
    const unsigned int decompressed_size = tile_size * tile_size * tile_width * tile_height;
    unsigned int bits_decompressed = 0u;
    enum class State: std::uint8_t {RLE=0, DATA=1};
    auto state = static_cast<State>(bits(1));

    auto buffer = Output_bitstream{decompression_buffer};

    while(true)
    {
        if(state == State::RLE)
        {
            std::uint16_t magnitude{0u};
            unsigned int bit_count = 0u;
            do
            {
                if(magnitude & 0x8000u)
                    throw std::runtime_error{"Error reading Pkmn sprite: overlong RLE packet"};
                magnitude <<= 1;
                magnitude |= bits(1);
                ++bit_count;
            } while(magnitude & 0x1);

            auto value = bits.read<std::uint16_t>(bit_count);
            value += magnitude + 1;

            bits_decompressed += 2 * value;
            if(bits_decompressed > decompressed_size)
            {
                if(fixed_buffer)
                {
                    value -= bits_decompressed - decompressed_size;
                    bits_decompressed = decompressed_size;
                }
                else
                    throw std::runtime_error{"Error reading Pkmn sprite: too much data decompressed"};
            }

            // std::cout<<"RLE 00 x "<<value<<" bits written "<<bits_decompressed<<'\n';
            for(auto i = 0u; i < value; ++i)
                buffer(0, 2);

            if(bits_decompressed == decompressed_size)
                break;

            state = State::DATA;
        }
        else if(state == State::DATA)
        {
            auto pair = bits(2);
            if(pair == 0)
            {
                state = State::RLE;
                continue;
            }
            bits_decompressed += 2;
            // std::cout<<"Data: "<<(int)pair<<" bits written "<<bits_decompressed<<'\n';
            buffer(pair, 2);
            if(bits_decompressed == decompressed_size)
                break;
        }
    }
}

void delta_decode(std::uint8_t * buffer, std::uint8_t tile_width, std::uint8_t tile_height)
{
    for(auto row = 0u; row < tile_height * tile_size; ++row)
    {
        std::uint8_t state = 0;
        for(auto col = 0u; col < tile_width * tile_size; col += 2)
        {
            for(auto i = 0u; i < 2u; ++i)
            {
                auto byte_ind = col * tile_height + row / (tile_size / 2);
                auto & pix = buffer[byte_ind];

                auto bit_ind = tile_size - row % (tile_size / 2) * 2 - 1 -i;
                auto val = (pix >> (bit_ind)) & 0x01u;

                if(val)
                    state = !state;

                if(state)
                    pix |= 1u << (bit_ind);
                else
                    pix &= ~(1u << (bit_ind));
            }
        }
    }
}

void xor_buf(std::uint8_t * dst, std::uint8_t * src, std::uint8_t tile_width, std::uint8_t tile_height)
{
    for(auto i = 0u; i < tile_width * tile_height * tile_size; ++i)
        dst[i] = dst[i] ^ src[i];
}

void Pkmn::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        auto bits = Input_bitstream{std::istreambuf_iterator<char>{input}};

        auto tile_width = bits(4);
        auto tile_height = bits(4);

        if(tile_width == 0 || tile_height == 0)
            throw std::runtime_error{"Error reading Pkmn sprite: 0 dimension"};

        auto primary_buffer = bits(1); // 0: BP0 in B, 1: BP0 in C0

        const auto buffer_stride = tile_size * (fixed_buffer_ ? fixed_tile_size * fixed_tile_size : tile_width * tile_height);
        auto decompression_buffer = std::vector<std::uint8_t>(2u * buffer_stride + tile_size * tile_width * tile_height);
        auto buffer_a = std::data(decompression_buffer);
        auto buffer_b = std::data(decompression_buffer) + buffer_stride;
        auto buffer_c = std::data(decompression_buffer) + 2u * buffer_stride;

        decompress(bits, tile_width, tile_height, primary_buffer ? buffer_c : buffer_b, fixed_buffer_);

        auto mode = bits(1);
        if(mode == 1)
            mode = 0x2 | bits(1);
        // 0: delta BP1, BP0
        // 2: delta BP0, XOR into BP1
        // 3: delta BP1, BP0, XOR into BP1

        std::cout<<(int)tile_width<<'x'<<(int)tile_height<<" pbuf: "<<(int)primary_buffer<<" mode: "<<(int)mode<<'\n';

        decompress(bits, tile_width, tile_height, primary_buffer ? buffer_b : buffer_c, fixed_buffer_);

        if(mode == 0 || mode == 3)
            delta_decode(buffer_b, tile_width, tile_height);
        delta_decode(buffer_c, tile_width, tile_height);
        if(mode == 2 || mode == 3)
            xor_buf(buffer_b, buffer_c, tile_width, tile_height);

        auto pocket_palette = std::array<Color, 4>{Color{0x00}, Color{0x54}, Color{0xA9}, Color{0xFF}};

        set_size(tile_width * tile_size, tile_height * tile_size);
        auto bp0 = Input_bitstream{buffer_b};
        auto bp1 = Input_bitstream{buffer_c};
        for(auto col = 0u; col < width_; col += 2)
        {
            for(auto row = 0u; row < height_; ++row)
            {
                image_data_[row][col] = pocket_palette[(bp1(1) << 1) | bp0(1)];
                image_data_[row][col + 1] = pocket_palette[(bp1(1) << 1) | bp0(1)];
                // image_data_[row][col + 1] = Color{static_cast<std::uint8_t>(out(1) * 255u)};
                // std::cout<<static_cast<int>(decompression_buffer[row * width_ + col] * 255u)<<'\n';
            }
        }
    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading Pkmn sprite: could not read file"};
        else
            throw std::runtime_error{"Error reading Pkmn sprite: unexpected end of file"};
    }
}

void Pkmn::handle_extra_args(const Args & args)
{
    auto options = Sub_args{"Pokemon Gen 1 Sprite"};
    try
    {
        options.add_options()
            ("tile-width", "Override width for tile layout (necessary for glitches to show up as in-game) [1-15]", cxxopts::value<unsigned int>(), "WIDTH")
            ("tile-height", "Override height for tile layout (necessary for glitches to show up as in-game) [1-15]", cxxopts::value<unsigned int>(), "HEIGHT")
            ("fixed-buffer", "Limit decompression buffer to 56x56 (necessary for glitches to show up as in-game)");
        // TODO: palette options

        auto sub_args = options.parse(args.extra_args);

        if(( sub_args.count("tile-width") && !sub_args.count("tile-height")) ||
           (!sub_args.count("tile-width") &&  sub_args.count("tile-height")))
        {
            throw std::runtime_error{options.help(args.help_text) + "\nMustn't specify --tile-width with --tile-height together"};
        }
        else if(sub_args.count("tile-width") && sub_args.count("tile-height"))
        {
            override_tile_width_ = sub_args["tile-width"].as<unsigned int>();
            override_tile_height_ = sub_args["tile-height"].as<unsigned int>();

            if(override_tile_width_ == 0 || override_tile_width_ > 15)
                throw std::runtime_error{options.help(args.help_text) + "\n--tile-width out of range [1-15]"};
            if(override_tile_height_ == 0 || override_tile_height_ > 15)
                throw std::runtime_error{options.help(args.help_text) + "\n--tile-height out of range [1-15]"};
        }

        fixed_buffer_ = sub_args.count("fixed-buffer");
    }
    catch(const cxxopts::OptionException & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}

void Pkmn::write(std::ostream & out, const Image & img, bool invert)
{
}
