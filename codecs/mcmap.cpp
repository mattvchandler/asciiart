#include "mcmap.hpp"

#include <cstdint>
#include <istream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <cstring>

#include <zlib.h>

#include "binio.hpp"

// using strings as output/input for ease of use with sstream, which we want for use with readb/writeb functuons
std::string zlib_decompress(std::istream & input)
{
    z_stream strm{};
    if(auto ret = inflateInit2(&strm, 16 + MAX_WBITS); ret != Z_OK)
        throw std::runtime_error(std::string{"Could not initialize ZLIB inflater: "} + strm.msg);

    std::string data;
    try
    {
        std::array<char, 4096> compressed_buffer;
        std::array<char, 4096> buffer;

        bool decompress_finished = false;
        while(input && !decompress_finished)
        {
            input.read(std::data(compressed_buffer), std::size(compressed_buffer));

            if(input.bad())
                throw std::runtime_error {"Error reading input file"};

            auto buffer_len = input.gcount();

            strm.avail_in = buffer_len;
            strm.next_in = reinterpret_cast<unsigned char *>(std::data(compressed_buffer));

            do
            {
                strm.avail_out = std::size(buffer);
                strm.next_out = reinterpret_cast<unsigned char *>(std::data(buffer));

                auto ret = inflate(&strm, Z_NO_FLUSH);

                if(ret < 0)
                    throw std::runtime_error{std::string{"Error decompressing MCMap file: "} + strm.msg};
                else if(ret == Z_NEED_DICT)
                    throw std::runtime_error{"Error decompressing MCMap file: Dictionary needed"};

                data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + std::size(buffer) - strm.avail_out);
                if(ret == Z_STREAM_END)
                {
                    decompress_finished = true;
                    break;
                }
            }
            while(strm.avail_out == 0);
        }
        if(!decompress_finished)
            throw std::runtime_error {"Unexpected end of MCMap file"};
    }
    catch(...)
    {
        inflateEnd(&strm);
        throw;
    }

    inflateEnd(&strm);

    return data;
}

void zlib_compress(std::ostream & out, const std::string & data)
{
    z_stream strm{};
    if(auto ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY); ret != Z_OK)
        throw std::runtime_error(std::string{"Could not initialize ZLIB deflater: "} + strm.msg);

    try
    {
        std::vector<char> compressed_buffer(deflateBound(&strm, std::size(data)));

        strm.avail_in = std::size(data);
        strm.next_in = const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(std::data(data)));

        strm.avail_out = std::size(compressed_buffer);
        strm.next_out = reinterpret_cast<unsigned char *>(std::data(compressed_buffer));

        if(auto ret = deflate(&strm, Z_FINISH); ret != Z_STREAM_END)
            throw std::runtime_error{std::string{"Error compressing MCMap file: "} + strm.msg};

        out.write(std::data(compressed_buffer), std::size(compressed_buffer) - strm.avail_out);
    }
    catch(...)
    {
        deflateEnd(&strm);
        throw;
    }

    deflateEnd(&strm);
}

const std::array mc_palette
{
    Color{  0,   0,   0,   0},
    Color{  0,   0,   0,   0},
    Color{  0,   0,   0,   0},
    Color{  0,   0,   0,   0},
    Color{ 89, 125,  39, 255},
    Color{109, 153,  48, 255},
    Color{127, 178,  56, 255},
    Color{ 67,  94,  29, 255},
    Color{174, 164, 115, 255},
    Color{213, 201, 140, 255},
    Color{247, 233, 163, 255},
    Color{130, 123,  86, 255},
    Color{140, 140, 140, 255},
    Color{171, 171, 171, 255},
    Color{199, 199, 199, 255},
    Color{105, 105, 105, 255},
    Color{180,   0,   0, 255},
    Color{220,   0,   0, 255},
    Color{255,   0,   0, 255},
    Color{135,   0,   0, 255},
    Color{112, 112, 180, 255},
    Color{138, 138, 220, 255},
    Color{160, 160, 255, 255},
    Color{ 84,  84, 135, 255},
    Color{117, 117, 117, 255},
    Color{144, 144, 144, 255},
    Color{167, 167, 167, 255},
    Color{ 88,  88,  88, 255},
    Color{  0,  87,   0, 255},
    Color{  0, 106,   0, 255},
    Color{  0, 124,   0, 255},
    Color{  0,  65,   0, 255},
    Color{180, 180, 180, 255},
    Color{220, 220, 220, 255},
    Color{255, 255, 255, 255},
    Color{135, 135, 135, 255},
    Color{115, 118, 129, 255},
    Color{141, 144, 158, 255},
    Color{164, 168, 184, 255},
    Color{ 86,  88,  97, 255},
    Color{106,  76,  54, 255},
    Color{130,  94,  66, 255},
    Color{151, 109,  77, 255},
    Color{ 79,  57,  40, 255},
    Color{ 79,  79,  79, 255},
    Color{ 96,  96,  96, 255},
    Color{112, 112, 112, 255},
    Color{ 59,  59,  59, 255},
    Color{ 45,  45, 180, 255},
    Color{ 55,  55, 220, 255},
    Color{ 64,  64, 255, 255},
    Color{ 33,  33, 135, 255},
    Color{100,  84,  50, 255},
    Color{123, 102,  62, 255},
    Color{143, 119,  72, 255},
    Color{ 75,  63,  38, 255},
    Color{180, 177, 172, 255},
    Color{220, 217, 211, 255},
    Color{255, 252, 245, 255},
    Color{135, 133, 129, 255},
    Color{152,  89,  36, 255},
    Color{186, 109,  44, 255},
    Color{216, 127,  51, 255},
    Color{114,  67,  27, 255},
    Color{125,  53, 152, 255},
    Color{153,  65, 186, 255},
    Color{178,  76, 216, 255},
    Color{ 94,  40, 114, 255},
    Color{ 72, 108, 152, 255},
    Color{ 88, 132, 186, 255},
    Color{102, 153, 216, 255},
    Color{ 54,  81, 114, 255},
    Color{161, 161,  36, 255},
    Color{197, 197,  44, 255},
    Color{229, 229,  51, 255},
    Color{121, 121,  27, 255},
    Color{ 89, 144,  17, 255},
    Color{109, 176,  21, 255},
    Color{127, 204,  25, 255},
    Color{ 67, 108,  13, 255},
    Color{170,  89, 116, 255},
    Color{208, 109, 142, 255},
    Color{242, 127, 165, 255},
    Color{128,  67,  87, 255},
    Color{ 53,  53,  53, 255},
    Color{ 65,  65,  65, 255},
    Color{ 76,  76,  76, 255},
    Color{ 40,  40,  40, 255},
    Color{108, 108, 108, 255},
    Color{132, 132, 132, 255},
    Color{153, 153, 153, 255},
    Color{ 81,  81,  81, 255},
    Color{ 53,  89, 108, 255},
    Color{ 65, 109, 132, 255},
    Color{ 76, 127, 153, 255},
    Color{ 40,  67,  81, 255},
    Color{ 89,  44, 125, 255},
    Color{109,  54, 153, 255},
    Color{127,  63, 178, 255},
    Color{ 67,  33,  94, 255},
    Color{ 36,  53, 125, 255},
    Color{ 44,  65, 153, 255},
    Color{ 51,  76, 178, 255},
    Color{ 27,  40,  94, 255},
    Color{ 72,  53,  36, 255},
    Color{ 88,  65,  44, 255},
    Color{102,  76,  51, 255},
    Color{ 54,  40,  27, 255},
    Color{ 72,  89,  36, 255},
    Color{ 88, 109,  44, 255},
    Color{102, 127,  51, 255},
    Color{ 54,  67,  27, 255},
    Color{108,  36,  36, 255},
    Color{132,  44,  44, 255},
    Color{153,  51,  51, 255},
    Color{ 81,  27,  27, 255},
    Color{ 17,  17,  17, 255},
    Color{ 21,  21,  21, 255},
    Color{ 25,  25,  25, 255},
    Color{ 13,  13,  13, 255},
    Color{176, 168,  54, 255},
    Color{215, 205,  66, 255},
    Color{250, 238,  77, 255},
    Color{132, 126,  40, 255},
    Color{ 64, 154, 150, 255},
    Color{ 79, 188, 183, 255},
    Color{ 92, 219, 213, 255},
    Color{ 48, 115, 112, 255},
    Color{ 52,  90, 180, 255},
    Color{ 63, 110, 220, 255},
    Color{ 74, 128, 255, 255},
    Color{ 39,  67, 135, 255},
    Color{  0, 153,  40, 255},
    Color{  0, 187,  50, 255},
    Color{  0, 217,  58, 255},
    Color{  0, 114,  30, 255},
    Color{ 91,  60,  34, 255},
    Color{111,  74,  42, 255},
    Color{129,  86,  49, 255},
    Color{ 68,  45,  25, 255},
    Color{ 79,   1,   0, 255},
    Color{ 96,   1,   0, 255},
    Color{112,   2,   0, 255},
    Color{ 59,   1,   0, 255},
    // added in 1.12
    Color{147, 124, 113, 255},
    Color{180, 152, 138, 255},
    Color{209, 177, 161, 255},
    Color{110,  93,  85, 255},
    Color{112,  57,  25, 255},
    Color{137,  70,  31, 255},
    Color{159,  82,  36, 255},
    Color{ 84,  43,  19, 255},
    Color{105,  61,  76, 255},
    Color{128,  75,  93, 255},
    Color{149,  87, 108, 255},
    Color{ 78,  46,  57, 255},
    Color{ 79,  76,  97, 255},
    Color{ 96,  93, 119, 255},
    Color{112, 108, 138, 255},
    Color{ 59,  57,  73, 255},
    Color{131,  93,  25, 255},
    Color{160, 114,  31, 255},
    Color{186, 133,  36, 255},
    Color{ 98,  70,  19, 255},
    Color{ 72,  82,  37, 255},
    Color{ 88, 100,  45, 255},
    Color{103, 117,  53, 255},
    Color{ 54,  61,  28, 255},
    Color{112,  54,  55, 255},
    Color{138,  66,  67, 255},
    Color{160,  77,  78, 255},
    Color{ 84,  40,  41, 255},
    Color{ 40,  28,  24, 255},
    Color{ 49,  35,  30, 255},
    Color{ 57,  41,  35, 255},
    Color{ 30,  21,  18, 255},
    Color{ 95,  75,  69, 255},
    Color{116,  92,  84, 255},
    Color{135, 107,  98, 255},
    Color{ 71,  56,  51, 255},
    Color{ 61,  64,  64, 255},
    Color{ 75,  79,  79, 255},
    Color{ 87,  92,  92, 255},
    Color{ 46,  48,  48, 255},
    Color{ 86,  51,  62, 255},
    Color{105,  62,  75, 255},
    Color{122,  73,  88, 255},
    Color{ 64,  38,  46, 255},
    Color{ 53,  43,  64, 255},
    Color{ 65,  53,  79, 255},
    Color{ 76,  62,  92, 255},
    Color{ 40,  32,  48, 255},
    Color{ 53,  35,  24, 255},
    Color{ 65,  43,  30, 255},
    Color{ 76,  50,  35, 255},
    Color{ 40,  26,  18, 255},
    Color{ 53,  57,  29, 255},
    Color{ 65,  70,  36, 255},
    Color{ 76,  82,  42, 255},
    Color{ 40,  43,  22, 255},
    Color{100,  42,  32, 255},
    Color{122,  51,  39, 255},
    Color{142,  60,  46, 255},
    Color{ 75,  31,  24, 255},
    Color{ 26,  15,  11, 255},
    Color{ 31,  18,  13, 255},
    Color{ 37,  22,  16, 255},
    Color{ 19,  11,   8, 255},
    // added in 1.16
    Color{133,  33,  34, 255},
    Color{163,  41,  42, 255},
    Color{189,  48,  49, 255},
    Color{100,  25,  25, 255},
    Color{104,  44,  68, 255},
    Color{127,  54,  83, 255},
    Color{148,  63,  97, 255},
    Color{ 78,  33,  51, 255},
    Color{ 64,  17,  20, 255},
    Color{ 79,  21,  25, 255},
    Color{ 92,  25,  29, 255},
    Color{ 48,  13,  15, 255},
    Color{ 15,  88,  94, 255},
    Color{ 18, 108, 115, 255},
    Color{ 22, 126, 134, 255},
    Color{ 11,  66,  70, 255},
    Color{ 40, 100,  98, 255},
    Color{ 50, 122, 120, 255},
    Color{ 58, 142, 140, 255},
    Color{ 30,  75,  74, 255},
    Color{ 60,  31,  43, 255},
    Color{ 74,  37,  53, 255},
    Color{ 86,  44,  62, 255},
    Color{ 45,  23,  32, 255},
    Color{ 14, 127,  93, 255},
    Color{ 17, 155, 114, 255},
    Color{ 20, 180, 133, 255},
    Color{ 10,  95,  70, 255},
    // added in 1.17 // TODO; appoximate values, update when mc wiki updates
    Color{ 69,  69,  69, 255},
    Color{ 85,  85,  85, 255},
    Color{ 99,  99,  99, 255},
    Color{ 51,  51,  51, 255},
    Color{150, 122, 102, 255},
    Color{184, 148, 125, 255},
    Color{213, 173, 145, 255},
    Color{113,  91,  76, 255},
    Color{ 88, 116, 104, 255},
    Color{108, 142, 127, 255},
    Color{125, 165, 148, 255},
    Color{ 66,  87,  78, 255},
};

// Note:
// This does not aim to be a complete Minecraft NBT parser
// We are only interested in the bare minimum to extract image data from a map item file
// As such, we ignore the tree structure and most fields are ignored rather than read

enum class nbt_tag: std::uint8_t
{
    end = 0,
    byte,
    int16,
    int32,
    int64,
    float32,
    float64,
    byte_array,
    string,
    list,
    compound,
    int32_array,
    int64_array
};

auto nbt_read_tag(std::istream & input)
{
    nbt_tag tag_type{};
    std::string name;

    readb(input, tag_type);

    if(tag_type != nbt_tag::end)
    {
        std::uint16_t name_length{};
        readb(input, name_length, binio_endian::BE);

        name = readstr(input, name_length);
    }

    return std::pair{tag_type, name};
}

void nbt_write_string(std::ostream & out, const std::string_view & str)
{
    writeb(out, static_cast<std::uint16_t>(std::size(str)), binio_endian::BE);
    writestr(out, str);
}
void nbt_write_tag(std::ostream & out, nbt_tag tag_type, const std::string_view & name)
{
    writeb(out, tag_type);
    if(tag_type != nbt_tag::end)
        nbt_write_string(out, name);
}

void nbt_skip_payload(std::istream & input, nbt_tag tag_type);

void nbt_skip_array(std::istream & input, unsigned int size)
{
    std::uint32_t len{};
    readb(input, len, binio_endian::BE);
    input.ignore(len * size);
}

std::vector<std::uint8_t> nbt_read_byte_array(std::istream & input)
{
    std::uint32_t len{};
    readb(input, len, binio_endian::BE);
    std::vector<std::uint8_t> data(len);
    input.read(reinterpret_cast<char *>(std::data(data)), std::size(data));
    return data;
}

void nbt_skip_string(std::istream & input)
{
    std::uint16_t len{};
    readb(input, len, binio_endian::BE);
    input.ignore(len);
}

void nbt_skip_compound(std::istream & input)
{
    while(true)
    {
        auto [tag_type, name] = nbt_read_tag(input);

        if(tag_type == nbt_tag::end)
            return;

        nbt_skip_payload(input, tag_type);
    }
}

void nbt_skip_list(std::istream & input)
{
    nbt_tag tag_type{};
    uint32_t len{};
    readb(input, tag_type);
    readb(input, len, binio_endian::BE);

    for(unsigned int i = 0; i < len; ++i)
        nbt_skip_payload(input, tag_type);
}

void nbt_write_empty_list(std::ostream & out)
{
    writeb(out, nbt_tag::end);
    writeb(out, std::int32_t{0});
}

void nbt_skip_payload(std::istream & input, nbt_tag tag_type)
{
    switch(tag_type)
    {
        case nbt_tag::end:
            break;
        case nbt_tag::byte:
            input.ignore(1);
            break;
        case nbt_tag::int16:
            input.ignore(2);
            break;
        case nbt_tag::int32:
            input.ignore(4);
            break;
        case nbt_tag::int64:
            input.ignore(8);
            break;
        case nbt_tag::float32:
            input.ignore(4);
            break;
        case nbt_tag::float64:
            input.ignore(8);
            break;
        case nbt_tag::byte_array:
            nbt_skip_array(input, 1);
            break;
        case nbt_tag::string:
            nbt_skip_string(input);
            break;
        case nbt_tag::list:
            nbt_skip_list(input);
            break;
        case nbt_tag::compound:
            nbt_skip_compound(input);
            break;
        case nbt_tag::int32_array:
            nbt_skip_array(input, 4);
            break;
        case nbt_tag::int64_array:
            nbt_skip_array(input, 8);
            break;
    }
}

struct Map_img
{
    std::vector<std::uint8_t> colors;
    std::int16_t width{128}, height{128};
};

auto nbt_read_map(std::istream & input)
{
    Map_img img;

    // walk the tree, skip anything not interesting
    while(input)
    {
        auto [tag_type, name] = nbt_read_tag(input);

        if(tag_type == nbt_tag::end || tag_type == nbt_tag::compound)
            // we're not interested in nbt structure, so just keep walking the next level down
            continue;

        // the spec guarantees that our interesting tags will not be in a list, so we only look for them in our walk
        if(name == "width" && tag_type == nbt_tag::int16)
            readb(input, img.width, binio_endian::BE);
        else if(name == "height" && tag_type == nbt_tag::int16)
            readb(input, img.height, binio_endian::BE);
        else if(name == "colors" && tag_type == nbt_tag::byte_array)
            img.colors = nbt_read_byte_array(input);
        else
            nbt_skip_payload(input, tag_type);
    }

    if(std::empty(img.colors))
        throw std::runtime_error{"no colors array found in file (is this actually an map item .dat file?)"};

    return img;
}

MCMap::MCMap(std::istream & input)
{
    auto dc=zlib_decompress(input);
    std::istringstream decompressed_input{dc};

    auto map_img = nbt_read_map(decompressed_input);

    set_size(map_img.width, map_img.height);

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            if(auto index = map_img.colors[row * width_ + col]; index < std::size(mc_palette))
            {
                image_data_[row][col] = mc_palette[index];
            }
            else
            {
                image_data_[row][col] = mc_palette[0];
                std::cerr<<"Warning: MCMap index "<<static_cast<int>(index)<<" is out of range (0 - "<<std::size(mc_palette)<<")\n";
            }
        }
    }
}

void MCMap::write(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    auto scaled = img.scale(128, 128);

    std::unordered_map<Color, std::uint8_t> reverse_palette;
    for(std::size_t i = 0; i < std::size(mc_palette); ++i)
        reverse_palette[mc_palette[i]] = i;

    for(std::size_t row = 0; row < scaled.get_height(); ++row)
    {
        for(std::size_t col = 0; col < scaled.get_width(); ++col)
        {
            FColor fcolor {scaled[row][col]};

            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            scaled[row][col] = fcolor;
        }
    }

    scaled.dither(std::begin(mc_palette), std::end(mc_palette));

    std::vector<std::uint8_t> colors(scaled.get_width() * scaled.get_height());
    for(std::size_t row = 0; row < scaled.get_height(); ++row)
    {
        for(std::size_t col = 0; col < scaled.get_width(); ++col)
        {
            colors[row * scaled.get_width() + col] = reverse_palette[scaled[row][col]];
        }
    }

    std::ostringstream uncompressed_out;

    // slap together a minimal, unmarked, and locked map
    nbt_write_tag(uncompressed_out, nbt_tag::compound, "");
    nbt_write_tag(uncompressed_out, nbt_tag::compound, "data");

    nbt_write_tag(uncompressed_out, nbt_tag::int32, "zCenter");
    writeb(uncompressed_out, std::int32_t{0}, binio_endian::BE);

    nbt_write_tag(uncompressed_out, nbt_tag::byte, "unlimitedTracking");
    writeb(uncompressed_out, std::uint8_t{0});

    nbt_write_tag(uncompressed_out, nbt_tag::byte, "trackingPosition");
    writeb(uncompressed_out, std::uint8_t{0});

    nbt_write_tag(uncompressed_out, nbt_tag::list, "frames");
    nbt_write_empty_list(uncompressed_out);

    nbt_write_tag(uncompressed_out, nbt_tag::byte, "scale");
    writeb(uncompressed_out, std::uint8_t{0});

    nbt_write_tag(uncompressed_out, nbt_tag::byte, "locked");
    writeb(uncompressed_out, std::uint8_t{1});

    nbt_write_tag(uncompressed_out, nbt_tag::string, "dimension");
    nbt_write_string(uncompressed_out, "minecraft:overworld");

    nbt_write_tag(uncompressed_out, nbt_tag::list, "banners");
    nbt_write_empty_list(uncompressed_out);

    nbt_write_tag(uncompressed_out, nbt_tag::int32, "xCenter");
    writeb(uncompressed_out, std::int32_t{0}, binio_endian::BE);

    nbt_write_tag(uncompressed_out, nbt_tag::byte_array, "colors");
    writeb(uncompressed_out, static_cast<std::int32_t>(std::size(colors)), binio_endian::BE);
    uncompressed_out.write(reinterpret_cast<char*>(std::data(colors)), std::size(colors));

    nbt_write_tag(uncompressed_out, nbt_tag::end, "");

    nbt_write_tag(uncompressed_out, nbt_tag::int32, "DataVersion");
    writeb(uncompressed_out, std::int32_t{2730}, binio_endian::BE); // 2730 is 1.17.1

    nbt_write_tag(uncompressed_out, nbt_tag::end, "");

    zlib_compress(out, uncompressed_out.str());
}
