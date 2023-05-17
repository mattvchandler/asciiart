#include "pkmn_gen2.hpp"

#include <iostream>
#include <map>
#include <stdexcept>

#include "pkmn_gen1.hpp"
#include "sub_args.hpp"

constexpr auto tile_dims = 8u;
constexpr auto tile_bytes = 16u;

void process_cmd(std::istream & input, std::vector<std::uint8_t> & decompressed, std::uint8_t cmd, std::size_t length)
{
    switch(cmd)
    {
        case 0x0: // direct copy
        {
            for(auto i = 0u; i < length; ++i)
                decompressed.emplace_back(input.get());
            return;
        }
        case 0x1: // byte fill
        {
            auto v = input.get();
            for(auto i = 0u; i < length; ++i)
                decompressed.emplace_back(v);
            return;
        }
        case 0x2: // word fill
        {
            auto v1 = input.get();
            auto v2 = input.get();
            for(auto i = 0u; i < length; ++i)
                decompressed.emplace_back((i % 2 == 0) ? v1 : v2);
            return;
        }
        case 0x3: // zero fill
        {
            for(auto i = 0u; i < length; ++i)
                decompressed.emplace_back(0);
            return;
        }
        case 0x4: // repeat
        case 0x5: // bit-reverse repeat
        case 0x6: // backwards repeat
        {
            auto ay = static_cast<std::uint8_t>(input.get());
            auto a = ay >> 7;
            auto y = ay & 0x7fu;

            auto start = std::size_t{0};

            if(a)
                start = std::size(decompressed) - y - 1;
            else
                start = (y * 0x100) + input.get();

            if(start >= std::size(decompressed))
                throw std::runtime_error{"Pkmn_gen2: start address out of range"};

            switch(cmd)
            {
                case 0x4: // repeat
                    for(auto i = start; i < start + length; ++i)
                    {
                        if(i >= std::size(decompressed))
                            throw std::runtime_error{"Pkmn_gen2: end address out of range"};
                        decompressed.emplace_back(decompressed[i]);
                    }
                    return;

                case 0x5: // bit-reverse repeat
                {
                    if(start + length >= std::size(decompressed))
                        throw std::runtime_error{"Pkmn_gen2: end address out of range"};

                    auto reversebits = [](std::uint8_t b)
                    {
                        auto b2 = decltype(b){0};
                        for(auto i = 0; i < 8; ++i)
                        {
                            b2 <<= 1;
                            b2 |= b & 0x01u;
                            b >>= 1;
                        }
                        return b2;
                    };

                    for(auto i = start; i < start + length; ++i)
                    {
                        if(i >= std::size(decompressed))
                            throw std::runtime_error{"Pkmn_gen2: end address out of range"};
                        decompressed.emplace_back(reversebits(decompressed[i]));
                    }

                    return;
                }

                case 0x6: // backwards repeat
                    for(auto i = start + 1; i-- > start - length + 1;)
                    {
                        if(i >= std::size(decompressed))
                            throw std::runtime_error{"Pkmn_gen2: end address out of range"};
                        decompressed.emplace_back(decompressed[i]);
                    }
                    return;
            }
            return;
        }
        case 0x7: // long header
        {
            auto sub_cmd = static_cast<std::uint8_t>(((length - 1) & 0x1cu) >> 2);
            auto sub_length = (((length - 1) & 0x3u) << 8 | input.get()) + 1;

            if(sub_cmd == 0x07u)
                throw std::runtime_error{"Pkmn_gen2 LZ3 sub-command is 0x07"};

            process_cmd(input, decompressed, sub_cmd, sub_length);
            return;
        }

        default:
            throw std::logic_error{"Pkmn_gen2: Invalid cmd code"};
    }
}

std::vector<std::uint8_t> lz3_decompress(std::istream & input)
{
    std::vector<std::uint8_t> decompressed;

    while(true)
    {
        auto header = static_cast<std::uint8_t>(input.get());
        if(header == 0xff)
            return decompressed;

        auto cmd = (header & 0xe0) >> 5;
        auto length = (header & 0x1f) + 1;

        process_cmd(input, decompressed, cmd, length);
    }

    return decompressed;
}

void Pkmn_gen2::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        // So... This isn't actually a real format.
        // The gen2 pokemon games store the pokemon sprites as LZ3 compressed 2bpp tiles
        // To lay the tiles out correctly requires the size, which is stored seperately
        // the palette is also stored seperately.
        // For gen1, this wasn't a problem. a redundant copy of the size was stored alongdide the image data,
        // and the palette was set system-wide (grayscale for the original GB
        // and pocket, and one of a handful of presets on GBC/GBA. The super
        // gameboy was a bit of an exception, because the game stores a palette index for each mon)

        // to make this usable, we have 3 options (and we're going to support them all)
        // 1. guess the layout and use a grayscale palette. The games only actually use a handful of sprite sizes, and we can figure out which it is from the # of tiles.
        // 2. allow setting the size and/or palette from Sub_args
        // 3. an LZ3 stream ends with an 0xFF byte. If our input begins with 0xFF, assume the next byte is dimensions, and the next 12 bytes is a 4-entry RGB palette, and the LZ3 data follows

        // 3 overrides 1, and 2 overrides both 3 and 1

        auto tile_width = static_cast<std::uint8_t>(tile_width_);
        auto tile_height = static_cast<std::uint8_t>(tile_height_);

        auto detect_header = input.get();
        if(detect_header == 0xff)
        {
            auto size = static_cast<std::uint8_t>(input.get());
            if(tile_width == 0 || tile_height == 0)
            {
                tile_width = size >> 4;
                tile_height = size & 0xf;
            }

            for(auto i = 0; i < 12; ++i)
            {
                if(palette_set_)
                    input.get();
                else
                    palette_entries_[i / 3][i % 3] = input.get();
            }
            palette_set_ = true;
        }
        else
            input.putback(detect_header);

        auto tiles = lz3_decompress(input);

        if(std::size(tiles) % tile_bytes != 0)
            throw std::runtime_error{"Pkmn_gen2 decompressed sprite data has odd size (" + std::to_string(std::size(tiles)) + " bytes)"};

        if(tile_width == 0 || tile_height == 0)
        {
            auto num_tiles = std::size(tiles) / tile_bytes;
            const auto size_map = std::map<std::uint8_t, std::pair<std::uint8_t, std::uint8_t>> {
                {24, {6, 4}},
                {25, {5, 5}},
                {36, {6, 6}},
                {49, {7, 7}}
            };

            if(auto found = size_map.find(num_tiles); found != std::end(size_map))
            {
                std::tie(tile_width, tile_height) = found->second;
            }
            else
            {
                tile_width = 1;
                tile_height = num_tiles;
            }
        }

        while(tile_width * tile_height > std::size(tiles) / tile_bytes)
            tiles.emplace_back(0);

        set_size(tile_dims * tile_width, tile_dims * tile_height);

        if(!palette_set_)
            palette_entries_ = Pkmn_gen1::palettes.at("greyscale");

        for(auto i = 0u, tile_col = 0u, row = 0u; i < tile_width * tile_height * tile_bytes; i += 2u, ++row)
        {
            if(row == tile_height * tile_dims)
            {
                row = 0;
                ++tile_col;
            }

            for(auto col = 0u, b = 8u; b -- > 0; ++col)
            {
                auto bit0 = (tiles[i + 1] >> b) & 0x1;
                auto bit1 = (tiles[i + 0] >> b) & 0x1;
                image_data_[row][tile_col * tile_dims + col] = palette_entries_[bit1 << 1 | bit0];
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

void Pkmn_gen2::handle_extra_args(const Args & args)
{
    auto options = Sub_args{"Pokemon Gen 2 Sprite"};
    try
    {
        auto palette_list = std::string{};
        for(auto &&[s,_]: Pkmn_gen1::palettes)
        {
            if(!std::empty(palette_list))
                palette_list += ", ";
            palette_list += s;
        }

        options.add_options()
            ("tile-width",     "Specify width for tile layout [1-15]",  cxxopts::value<unsigned int>(), "WIDTH")
            ("tile-height",    "Specify height for tile layout [1-15]", cxxopts::value<unsigned int>(), "HEIGHT")
            ("palette",        "Palette to display or convert into. Valid values are: " + palette_list, cxxopts::value<std::string>(), "PALETTE")
            ("palette-colors", "Specify a comma-seperated list of palette RGB values [0-255]. 4 colors (12 values) should be specified. If 2 colors (6 values) are entered, they are assumed to be the middle 2 color indexes, and the first is assumed to be white and the last to be black (as is the case for in-game palettes). Overrides --palette",
                                                                        cxxopts::value<std::vector<unsigned int>>(), "COLORS");

        auto sub_args = options.parse(args.extra_args);

        if(( sub_args.count("tile-width") && !sub_args.count("tile-height")) ||
           (!sub_args.count("tile-width") &&  sub_args.count("tile-height")))
        {
            throw std::runtime_error{options.help(args.help_text) + "\nMust specify --tile-width and --tile-height together"};
        }
        else if(sub_args.count("tile-width") && sub_args.count("tile-height"))
        {
            tile_width_ = sub_args["tile-width"].as<unsigned int>();
            tile_height_ = sub_args["tile-height"].as<unsigned int>();

            if(tile_width_ == 0 || tile_width_ > 15)
                throw std::runtime_error{options.help(args.help_text) + "\n--tile-width out of range [1-15]"};
            if(tile_height_ == 0 || tile_height_ > 15)
                throw std::runtime_error{options.help(args.help_text) + "\n--tile-height out of range [1-15]"};
        }

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
            palette_set_ = true;
        }
        else if(sub_args.count("palette"))
        {
            auto palette = sub_args["palette"].as<std::string>();
            if(!Pkmn_gen1::palettes.contains(palette))
                throw std::runtime_error{options.help(args.help_text) + "\n'" + palette + "' is not valid for --palette. Valid values are: " + palette_list};

            palette_entries_ = Pkmn_gen1::palettes.at(palette);
            palette_set_ = true;
        }
    }
    catch(const cxxopt_exception & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}
