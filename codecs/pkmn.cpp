#include "pkmn.hpp"

#include <cstdint>
#include <iterator>

#include "bitstream.hpp"
#include "sub_args.hpp"

constexpr auto tile_size = 8u;
constexpr auto fixed_tile_size = 7u;

// constexpr auto green_palette = std::array<Color, 4>{Color{0xE0, 0xF8, 0xD0}, Color{0x88, 0xC0, 0x70}, Color{0x34, 0x68, 0x56}, Color{0x08, 0x18, 0x20}};
constexpr auto pocket_palette = std::array<Color, 4>{Color{0xFF}, Color{0xA9}, Color{0x54}, Color{0x00}};
// constexpr auto red_palette = std::array<Color, 4>{Color{0xFF, 0xEF, 0xFF}, Color{0xF7, 0xB5, 0x8C}, Color{0x84, 0x73, 0x9C}, Color{0x18, 0x10, 0x10}};
// constexpr auto blue_palette = std::array<Color, 4>{Color{0xFF, 0xFF, 0xFF}, Color{0x63, 0xA5, 0xFF}, Color{0x00, 0x00, 0xFF}, Color{0x00, 0x00, 0x00}};
constexpr auto mew_palette = std::array{Color{0xF8, 0xE8, 0xF8}, Color{0xF0, 0xB0, 0x88}, Color{0x80, 0x70, 0x98}, Color{0x18, 0x10, 0x10}};

void decompress(Input_bitstream<std::istreambuf_iterator<char>> & bits, std::uint8_t tile_width, std::uint8_t tile_height, std::uint8_t * decompression_buffer, bool check_overrun)
{
    const unsigned int decompressed_size = tile_size * tile_size * tile_width * tile_height;
    unsigned int bits_decompressed = 0u;
    enum class State: std::uint8_t {RLE=0, DATA=1};
    auto state = static_cast<State>(bits(1));

    // we're having the buffer laid out by columns. Each byte is a row in a tile, so each 8 bytes forms a tile. Unfortunately, the data is decompressed into 2 bit wide columns, so this fun gets the bits in the right places (or the console-acurate wrong places in the case of glitces)
    auto bit_pair_write = [tile_height, decompression_buffer, col = 0u, row = 0u](std::uint8_t bits) mutable
    {
        auto byte_ind = col / tile_size * tile_height * tile_size + row;
        auto bit_ind = col % tile_size;

        decompression_buffer[byte_ind] |= (bits & 0x03u) << (6u - bit_ind);

        if(++row == tile_height * tile_size)
        {
            row = 0u;
            col+= 2u;
        }
    };

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
                if(check_overrun)
                {
                    throw std::runtime_error{"Error reading Pkmn sprite: too much data decompressed"};
                }
                else
                {
                    value -= (bits_decompressed - decompressed_size) / 2;
                    bits_decompressed = decompressed_size;
                }
            }

            for(auto i = 0u; i < value; ++i)
                bit_pair_write(0);

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
            bit_pair_write(pair);
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
        for(auto col = 0u; col < tile_width * tile_size; col += tile_size)
        {
            auto & pix = buffer[col / tile_size * tile_height * tile_size + row];
            for(auto i = 0u; i < tile_size; ++i)
            {
                auto bit_ind = 7u - i;
                auto val = (pix >> (bit_ind)) & 0x01;
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

void copy_and_arrange_buf(std::uint8_t * dst, std::uint8_t * src, std::uint8_t tile_width, std::uint8_t tile_height)
{
    for(auto i = 0u; i < fixed_tile_size * fixed_tile_size * tile_size; ++i)
        dst[i] = 0u;

    std::uint8_t y_offset = std::uint8_t{fixed_tile_size} - tile_height;
    std::uint8_t x_offset = (std::uint8_t{fixed_tile_size} - tile_width + 1) / 2u;

    std::uint8_t tile_offset = std::uint8_t{fixed_tile_size} * x_offset + y_offset;
    std::uint8_t byte_offset = std::uint8_t{tile_size} * tile_offset;

    for(auto tile_col = 0u; tile_col < tile_width; ++tile_col)
    {
        for(auto row = 0u; row < tile_height * tile_size; ++row)
        {
            auto src_ind = tile_col * tile_height * tile_size + row;
            auto dst_ind = byte_offset + tile_col * fixed_tile_size * tile_size + row;
            dst[dst_ind] = src[src_ind];
        }
    }
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

        const auto buffer_stride = tile_size * fixed_tile_size * fixed_tile_size;
        auto decompression_buffer = std::vector<std::uint8_t>(2u * buffer_stride + tile_size * std::max(static_cast<unsigned int>(tile_width * tile_height), fixed_tile_size * fixed_tile_size));

        auto buffer_a = std::data(decompression_buffer);
        auto buffer_b = std::data(decompression_buffer) + buffer_stride;
        auto buffer_c = std::data(decompression_buffer) + 2u * buffer_stride;

        decompress(bits, tile_width, tile_height, primary_buffer ? buffer_c : buffer_b, check_overrun_);

        // Modes:
        // 0: delta BP1, BP0
        // 2: delta BP0, XOR into BP1
        // 3: delta BP1, BP0, XOR into BP1
        auto mode = bits(1);
        if(mode == 1)
            mode = 0x2 | bits(1);

        decompress(bits, tile_width, tile_height, primary_buffer ? buffer_b : buffer_c, check_overrun_);

        if(primary_buffer)
        {
            if(mode == 0 || mode == 3)
                delta_decode(buffer_b, tile_width, tile_height);
            delta_decode(buffer_c, tile_width, tile_height);
            if(mode == 2 || mode == 3)
                xor_buf(buffer_b, buffer_c, tile_width, tile_height);
        }
        else
        {
            if(mode == 0 || mode == 3)
                delta_decode(buffer_c, tile_width, tile_height);
            delta_decode(buffer_b, tile_width, tile_height);
            if(mode == 2 || mode == 3)
                xor_buf(buffer_c, buffer_b, tile_width, tile_height);
        }

        if(override_tile_width_ && override_tile_height_)
        {
            copy_and_arrange_buf(buffer_a, buffer_b, override_tile_width_, override_tile_height_);
            copy_and_arrange_buf(buffer_b, buffer_c, override_tile_width_, override_tile_height_);
        }
        else
        {
            copy_and_arrange_buf(buffer_a, buffer_b, tile_width, tile_height);
            copy_and_arrange_buf(buffer_b, buffer_c, tile_width, tile_height);
        }

        // TODO: fit to sprite
        // set_size(fixed_tile_size * tile_size * 3, fixed_tile_size * tile_size);

        // for(auto row = 0u; row < height_; ++row)
        // {
        //     for(auto col = 0u; col < width_; col += tile_size)
        //     {
        //         auto byte = decompression_buffer[col / tile_size * fixed_tile_size * tile_size + row];
        //         for(auto i = 0u; i < tile_size; ++i)
        //         {
        //             auto bit = (byte >> (7u - i)) & 0x01;
        //             image_data_[row][col + i] = Color{static_cast<std::uint8_t>(255u - bit * 255u)};
        //         }
        //     }
        // }
        set_size(fixed_tile_size * tile_size, fixed_tile_size * tile_size);

        for(auto row = 0u; row < height_; ++row)
        {
            for(auto col = 0u; col < width_; col += tile_size)
            {
                auto byte_ind = col / tile_size * fixed_tile_size * tile_size + row;
                auto byte0 = buffer_a[byte_ind];
                auto byte1 = buffer_b[byte_ind];
                for(auto i = 0u; i < tile_size; ++i)
                {
                    auto bit_ind = 7u - i;
                    auto bit0 = (byte0 >> bit_ind) & 0x01;
                    auto bit1 = (byte1 >> bit_ind) & 0x01;
                    image_data_[row][col + i] = mew_palette[bit1 << 1 | bit0];
                }
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
            ("fixed-buffer", "Limit decompression buffer to 56x56 (necessary for glitches to show up as in-game)")
            ("allow-overrun", "Continue decoding image when too more data is decompressed than expected (necessary for glitches to show up as in-game)");
        // TODO: palette options

        auto sub_args = options.parse(args.extra_args);

        if(( sub_args.count("tile-width") && !sub_args.count("tile-height")) ||
           (!sub_args.count("tile-width") &&  sub_args.count("tile-height")))
        {
            throw std::runtime_error{options.help(args.help_text) + "\nMust specify --tile-width and --tile-height together"};
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

        check_overrun_ = !sub_args.count("allow-overrun");
        fixed_buffer_ = sub_args.count("fixed-buffer");
    }
    catch(const cxxopts::OptionException & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}

void Pkmn::write(std::ostream & out, const Image & img, bool invert)
{
    out<<"lol"<<img.get_width()<<invert<<'\n';
}
