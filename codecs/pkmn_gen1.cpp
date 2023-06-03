#include "pkmn_gen1.hpp"

#include <iterator>
#include <unordered_map>

#include <cmath>

#include "bitstream.hpp"
#include "sub_args.hpp"

constexpr auto tile_dims = 8u;

const std::map<std::string, std::array<Color, 4>> Pkmn_gen1::palettes {
    // pure greyscale. Approximately what you would see on a Gameboy Pocket
    {"greyscale", {Color{0xFF},             Color{0xA9},             Color{0x54},             Color{0x00}}},
    // Approximately what you would see on the original Gameboy's green LCD
    {"gb_green",  {Color{0xE0, 0xF8, 0xD0}, Color{0x88, 0xC0, 0x70}, Color{0x34, 0x68, 0x56}, Color{0x08, 0x18, 0x20}}},
    // default palettes used by the GBC for pokemon Red and Blue (though the player can change the palette used before the game starts)
    {"gbc_red",   {Color{0xFF, 0xFF, 0xFF}, Color{0xFF, 0x84, 0x84}, Color{0x94, 0x3A, 0x3A}, Color{0x00, 0x00, 0x00}}},
    {"gbc_blue",  {Color{0xFF, 0xFF, 0xFF}, Color{0x63, 0xA5, 0xFF}, Color{0x00, 0x00, 0xFF}, Color{0x00, 0x00, 0x00}}},
    // palettes used by the SGB for pokemon sprites
    {"green",     {Color{0xF8, 0xE8, 0xF8}, Color{0xA0, 0xD0, 0x80}, Color{0x48, 0xA0, 0x58}, Color{0x18, 0x10, 0x10}}},
    {"red",       {Color{0xF8, 0xE8, 0xF8}, Color{0xF8, 0xA0, 0x50}, Color{0xD0, 0x50, 0x30}, Color{0x18, 0x10, 0x10}}},
    {"cyan",      {Color{0xF8, 0xE8, 0xF8}, Color{0xA8, 0xC8, 0xE8}, Color{0x70, 0x98, 0xC8}, Color{0x18, 0x10, 0x10}}},
    {"yellow",    {Color{0xF8, 0xE8, 0xF8}, Color{0xF8, 0xE0, 0x70}, Color{0xD0, 0xA0, 0x00}, Color{0x18, 0x10, 0x10}}},
    {"brown",     {Color{0xF8, 0xE8, 0xF8}, Color{0xE0, 0xA0, 0x78}, Color{0xA8, 0x70, 0x48}, Color{0x18, 0x10, 0x10}}},
    {"grey",      {Color{0xF8, 0xE8, 0xF8}, Color{0xD0, 0xA8, 0xB0}, Color{0x78, 0x78, 0x90}, Color{0x18, 0x10, 0x10}}},
    {"purple",    {Color{0xF8, 0xE8, 0xF8}, Color{0xD8, 0xB0, 0xC0}, Color{0xA8, 0x78, 0xB8}, Color{0x18, 0x10, 0x10}}},
    {"blue",      {Color{0xF8, 0xE8, 0xF8}, Color{0x90, 0xA0, 0xD8}, Color{0x58, 0x78, 0xB8}, Color{0x18, 0x10, 0x10}}},
    {"pink",      {Color{0xF8, 0xE8, 0xF8}, Color{0xF0, 0xB0, 0xC0}, Color{0xE0, 0x78, 0xA8}, Color{0x18, 0x10, 0x10}}},
    {"mew",       {Color{0xF8, 0xE8, 0xF8}, Color{0xF0, 0xB0, 0x88}, Color{0x80, 0x70, 0x98}, Color{0x18, 0x10, 0x10}}},
    // We could add Pokemon Yellow's palettes, but, A) They're very similar to the SGB palettes, and B) I like the SGB palettes better
};

template<Byte_input_iter InputIter>
void decompress(Input_bitstream<InputIter> & bits, std::uint8_t tile_width, std::uint8_t tile_height, std::uint8_t * decompression_buffer, bool check_overrun)
{
    const unsigned int decompressed_size = tile_dims * tile_dims * tile_width * tile_height;
    unsigned int bits_decompressed = 0u;
    enum class State: std::uint8_t {RLE=0, DATA=1};
    auto state = static_cast<State>(bits(1));

    // we're having the buffer laid out by columns. Each byte is a row in a tile, so each 8 bytes forms a tile. Unfortunately, the data is decompressed into 2 bit wide columns, so this fun gets the bits in the right places (or the console-acurate wrong places in the case of glitches)
    auto bit_pair_write = [tile_height, decompression_buffer, col = 0u, row = 0u](std::uint8_t bits) mutable
    {
        auto byte_ind = col / tile_dims * tile_height * tile_dims + row;
        auto bit_ind = col % tile_dims;

        decompression_buffer[byte_ind] |= (bits & 0x03u) << (6u - bit_ind);

        if(++row == tile_height * tile_dims)
        {
            row = 0u;
            col += 2u;
        }
    };

    while(true)
    {
        if(state == State::RLE)
        {
            std::uint8_t bit_count = 1u;
            while(true)
            {
                auto b = bits(1);
                if(!b)
                    break;
                ++bit_count;
            }

            auto value = bits.template read<std::uint16_t>(bit_count);
            value += (1 << bit_count) - 1;

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

template<Byte_output_iter OutputIter>
void compress(Output_bitstream<OutputIter> & bits, std::uint8_t tile_width, std::uint8_t tile_height, std::uint8_t * compression_buffer)
{
    enum class State: std::uint8_t {RLE=0, DATA=1} state = State::RLE;
    std::uint16_t rle_run = 0u;
    for(auto col = 0u; col < tile_width * tile_dims; col += 2)
    {
        for(auto row = 0u; row < tile_height * tile_dims; ++row)
        {
            auto byte_ind = col / tile_dims * tile_height * tile_dims + row;
            auto bit_ind = col % tile_dims;

            auto p = (compression_buffer[byte_ind] >> (6u - bit_ind)) & 0x3u;

            if(row == 0u && col == 0u)
            {
                if(p)
                    state = State::DATA;
                else
                    state = State::RLE;

                bits(static_cast<std::underlying_type_t<State>>(state), 1);
            }

            if(state == State::RLE)
            {
                ++rle_run;
                if(p)
                {
                    std::uint16_t bit_width{0u};
                    for(auto x = rle_run; x; x >>= 1u, ++bit_width);
                    --bit_width;
                    auto v = rle_run ^ (1u << bit_width);
                    auto l = rle_run - v - 2u;

                    bits(l, bit_width);
                    bits(v, bit_width);

                    state = State::DATA;
                    bits(p, 2u);
                }
            }
            else if(state == State::DATA)
            {
                if(p)
                {
                    bits(p, 2u);
                }
                else
                {
                    bits(0u, 2u);
                    state = State::RLE;
                    rle_run = 1u;
                }
            }
        }
    }
    if(state == State::RLE)
    {
        ++rle_run;

        std::uint16_t bit_width{0u};
        for(auto x = rle_run; x; x >>= 1u, ++bit_width);
        --bit_width;
        auto v = rle_run ^ (1u << bit_width);
        auto l = rle_run - v - 2u;

        bits(l, bit_width);
        bits(v, bit_width);
    }
}

void delta_code(std::uint8_t * buffer, std::uint8_t tile_width, std::uint8_t tile_height, bool encode)
{
    for(auto row = 0u; row < tile_height * tile_dims; ++row)
    {
        std::uint8_t state = 0u;
        for(auto col = 0u; col < tile_width * tile_dims; col += tile_dims)
        {
            auto & pix = buffer[col / tile_dims * (tile_height != 32 ? tile_height : 0) * tile_dims + row];
            for(auto i = 0u; i < tile_dims; ++i)
            {
                auto bit_ind = 7u - i;
                auto val = (pix >> (bit_ind)) & 0x01u;

                bool output;

                if(encode)
                {
                    output = val != state;
                    state = val;
                }
                else
                {
                    if(val)
                        state = !state;
                    output = state;
                }

                if(output)
                    pix |= 1u << (bit_ind);
                else
                    pix &= ~(1u << (bit_ind));
            }
        }
    }
}

void xor_buf(std::uint8_t * dst, std::uint8_t * src, std::uint8_t tile_width, std::uint8_t tile_height)
{
    for(auto i = 0u; i < tile_width * tile_height * tile_dims; ++i)
        dst[i] = dst[i] ^ src[i];
}

void copy_and_arrange_buf(std::uint8_t * dst, std::uint8_t * src, std::uint8_t tile_width, std::uint8_t tile_height, std::uint8_t buffer_tile_width, std::uint8_t buffer_tile_height)
{
    for(auto i = 0u; i < buffer_tile_width * buffer_tile_height * tile_dims; ++i)
        dst[i] = 0u;

    std::uint8_t y_offset = buffer_tile_height - tile_height;
    std::uint8_t x_offset = static_cast<std::uint8_t>(buffer_tile_width - tile_width + 1) / 2u;

    std::uint8_t tile_offset = buffer_tile_height * x_offset + y_offset;
    std::uint8_t byte_offset = std::uint8_t{tile_dims} * tile_offset;

    for(auto tile_col = 0u; tile_col < tile_width; ++tile_col)
    {
        for(auto row = 0u; row < tile_height * tile_dims; ++row)
        {
            auto src_ind = tile_col * tile_height * tile_dims + row;
            auto dst_ind = byte_offset + tile_col * buffer_tile_height * tile_dims + row;
            dst[dst_ind] = src[src_ind];
        }
    }
}

void Pkmn_gen1::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        auto bits = Input_bitstream{std::istreambuf_iterator<char>{input}};

        auto tile_width = bits(4);
        auto tile_height = bits(4);

        if(check_overrun_)
        {
            if(tile_width == 0 || tile_height == 0)
                throw std::runtime_error{"Error reading Pkmn sprite: 0 dimension"};
        }
        else
        {
            if(tile_width == 0)
                tile_width = 32;
            if(tile_height == 0)
                tile_height = 32;
        }

        auto buffer_tile_width = fixed_buffer_ ? 7u : tile_width;
        auto buffer_tile_height = fixed_buffer_ ? 7u : tile_height;

        auto primary_buffer = bits(1); // 0: BP0 in B, 1: BP0 in C

        const auto buffer_stride = tile_dims * buffer_tile_width * buffer_tile_height;
        auto decompression_buffer = std::vector<std::uint8_t>(2u * buffer_stride + tile_dims * std::max({static_cast<unsigned int>(override_tile_width_ * override_tile_height_), static_cast<unsigned int>(tile_width * tile_height), buffer_tile_width * buffer_tile_height}));

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
            mode = 0x2u | bits(1);

        decompress(bits, tile_width, tile_height, primary_buffer ? buffer_b : buffer_c, check_overrun_);

        if(primary_buffer)
        {
            if(mode == 0u || mode == 3u)
                delta_code(buffer_b, tile_width, tile_height, false);
            delta_code(buffer_c, tile_width, tile_height, false);
            if(mode == 2u || mode == 3u)
                xor_buf(buffer_b, buffer_c, tile_width, tile_height);
        }
        else
        {
            if(mode == 0u || mode == 3u)
                delta_code(buffer_c, tile_width, tile_height, false);
            delta_code(buffer_b, tile_width, tile_height, false);
            if(mode == 2u || mode == 3u)
                xor_buf(buffer_c, buffer_b, tile_width, tile_height);
        }

        if(override_tile_width_ && override_tile_height_)
        {
            copy_and_arrange_buf(buffer_a, buffer_b, override_tile_width_, override_tile_height_, buffer_tile_width, buffer_tile_height);
            copy_and_arrange_buf(buffer_b, buffer_c, override_tile_width_, override_tile_height_, buffer_tile_width, buffer_tile_height);
        }
        else
        {
            copy_and_arrange_buf(buffer_a, buffer_b, tile_width, tile_height, buffer_tile_width, buffer_tile_height);
            copy_and_arrange_buf(buffer_b, buffer_c, tile_width, tile_height, buffer_tile_width, buffer_tile_height);
        }

        set_size(buffer_tile_width * tile_dims, buffer_tile_height * tile_dims);

        for(auto row = 0u; row < height_; ++row)
        {
            for(auto col = 0u; col < width_; col += tile_dims)
            {
                auto byte_ind = col / tile_dims * buffer_tile_height * tile_dims + row;
                auto byte0 = buffer_a[byte_ind];
                auto byte1 = buffer_b[byte_ind];
                for(auto i = 0u; i < tile_dims; ++i)
                {
                    auto bit_ind = 7u - i;
                    auto bit0 = (byte0 >> bit_ind) & 0x01;
                    auto bit1 = (byte1 >> bit_ind) & 0x01;
                    image_data_[row][col + i] = palette_entries_[bit1 << 1 | bit0];
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

void Pkmn_gen1::handle_extra_args(const Args & args)
{
    auto options = Sub_args{"Pokemon Gen 1 Sprite"};
    try
    {
        auto palette_list = std::string{};
        for(auto &&[s,_]: palettes)
        {
            if(!std::empty(palette_list))
                palette_list += ", ";
            palette_list += s;
        }

        options.add_options()
            ("tile-width",    "Override width for tile layout (necessary for glitches to show up as in-game) [1-15]", cxxopts::value<unsigned int>(), "WIDTH")
            ("tile-height",   "Override height for tile layout (necessary for glitches to show up as in-game) [1-15]", cxxopts::value<unsigned int>(), "HEIGHT")
            ("fixed-buffer",  "Limit decompression buffer to 56x56 (necessary for glitches to show up as in-game)")
            ("allow-overrun", "Continue decoding image when too more data is decompressed than expected (necessary for glitches to show up as in-game)")
            ("palette",       "Palette to display or convert into. Valid values are: " + palette_list, cxxopts::value<std::string>()->default_value("greyscale"), "PALETTE")
            ("palette-colors", "Specify a comma-seperated list of palette RGB values [0-255]. 4 colors (12 values) should be specified. If 2 colors (6 values) are entered, they are assumed to be the middle 2 color indexes, and the first is assumed to be white and the last to be black. Overrides --palette",
                                cxxopts::value<std::vector<unsigned int>>(), "COLORS");

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

        auto palette = sub_args["palette"].as<std::string>();
        if(!palettes.contains(palette))
            throw std::runtime_error{options.help(args.help_text) + "\n'" + palette + "' is not valid for --palette. Valid values are: " + palette_list};

        if(sub_args.count("palette-colors"))
        {
            auto & colors = sub_args["palette-colors"].as<std::vector<unsigned int>>();
            auto color_count = std::size(colors);

            if(color_count != 12 && color_count != 6)
                throw std::runtime_error{options.help(args.help_text) + "\nMust specify 12 or 6 RGB values for --palette-colors. " + std::to_string(color_count) + " specified"};

            for(auto i = 0u; i < color_count; ++i)
            {
                auto c = colors[i];
                if(c > 255)
                    throw std::runtime_error{options.help(args.help_text) + "\n--palette-colors entry (" + std::to_string(c) + ") out of range [0-255]"};

                palette_entries_[color_count == 12 ? i / 3 : i / 3 + 1][i % 3] = c;
            }

            if(color_count == 6)
            {
                palette_entries_[0] = Color{0xff};
                palette_entries_[3] = Color{0x00};
            }
        }
        else
            palette_entries_ = palettes.at(palette);
    }
    catch(const cxxopt_exception & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}

void Pkmn_gen1::write(std::ostream & out, const Image & img, bool invert)
{
    // TODO: maybe add output-tile-width output-tile-height flags

    unsigned int tile_width, tile_height;

    if(img.get_width() >= img.get_height())
    {
        tile_width = std::min(static_cast<unsigned int>(std::ceil(static_cast<float>(img.get_width()) / tile_dims)), true ? 7u : 15u);
        auto scale_factor = static_cast<float>(tile_width * tile_dims) / img.get_width();
        tile_height = static_cast<unsigned int>(std::ceil((img.get_height() * scale_factor) / tile_dims));
    }
    else
    {
        tile_width = std::min(static_cast<unsigned int>(std::ceil(static_cast<float>(img.get_height()) / tile_dims)), true ? 7u : 15u);
        auto scale_factor = static_cast<float>(tile_width * tile_dims) / img.get_height();
        tile_height = static_cast<unsigned int>(std::ceil((img.get_width() * scale_factor) / tile_dims));
    }

    auto scaled = img.scale(tile_width * tile_dims, tile_height * tile_dims);

    if(invert)
    {
        for(std::size_t row = 0u; row < scaled.get_height(); ++row)
        {
            for(std::size_t col = 0u; col < scaled.get_width(); ++col)
            {
                for(auto c = 0u; c < 3u; ++c)
                    scaled[row][col][c] = 255u - scaled[row][col][c];
            }
        }
    }

    scaled.dither(std::begin(palette_entries_), std::end(palette_entries_));

    auto reverse_palette = std::unordered_map<Color, std::uint8_t>{};
    for(std::size_t i = 0; i < std::size(palette_entries_); ++i)
        reverse_palette[palette_entries_[i]] = i;

    const auto buffer_stride = tile_dims * tile_width * tile_height;
    auto compression_buffer = std::vector<std::uint8_t>(2 * buffer_stride);

    auto buffer_b = std::data(compression_buffer);
    auto buffer_c = std::data(compression_buffer) + buffer_stride;
    for(auto tile_col = 0u; tile_col < tile_width; ++tile_col)
    {
        for(auto row = 0u; row < scaled.get_height(); ++row)
        {
            auto b0 = std::uint8_t{0}, b1 = std::uint8_t{0};
            for(auto b = 0u; b < tile_dims; ++b)
            {
                b0 <<= 1u;
                b1 <<= 1u;

                auto c = reverse_palette[scaled[row][tile_col * tile_dims + b]];

                b0 |= c & 0x01u;
                b1 |= (c >> 1) & 0x01u;
            }

            auto byte_ind = tile_col * tile_height * tile_dims + row;
            buffer_b[byte_ind] = b0;
            buffer_c[byte_ind] = b1;
        }
    }

    auto best_compressed_output = std::vector<std::uint8_t>{};
    for(auto && primary_buffer: {0u, 1u})
    {
        for(auto && mode: {0u, 2u, 3u})
        {
            auto buffer = compression_buffer;
            buffer_b = std::data(buffer);
            buffer_c = std::data(buffer) + buffer_stride;

            auto compressed_out = std::vector<std::uint8_t>{};
            auto bits = Output_bitstream{std::back_inserter(compressed_out)};

            bits(tile_width, 4);
            bits(tile_height, 4);
            bits(primary_buffer, 1);

            if(primary_buffer)
            {
                if(mode == 2u || mode == 3u)
                    xor_buf(buffer_b, buffer_c, tile_width, tile_height);
                delta_code(buffer_c, tile_width, tile_height, true);
                if(mode == 0u || mode == 3u)
                    delta_code(buffer_b, tile_width, tile_height, true);
            }
            else
            {
                if(mode == 2u || mode == 3u)
                    xor_buf(buffer_c, buffer_b, tile_width, tile_height);
                delta_code(buffer_b, tile_width, tile_height, true);
                if(mode == 0u || mode == 3u)
                    delta_code(buffer_c, tile_width, tile_height, true);
            }

            compress(bits, tile_width, tile_height, primary_buffer ? buffer_c : buffer_b);

            bits(mode, mode ? 2 : 1);

            compress(bits, tile_width, tile_height, primary_buffer ? buffer_b : buffer_c);
            bits.flush_current_byte();

            if(std::empty(best_compressed_output) || std::size(compressed_out) < std::size(best_compressed_output))
                best_compressed_output = std::move(compressed_out);
        }
    }

    out.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        out.write(reinterpret_cast<char *>(std::data(best_compressed_output)), std::size(best_compressed_output));
    }
    catch(std::ios_base::failure & e)
    {
        throw std::runtime_error{"Error writing Pkmn sprite: could not write file"};
    }
}
