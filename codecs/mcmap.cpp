#include "mcmap.hpp"

#include <cstdint>
#include <istream>
#include <stdexcept>
#include <sstream>

#include <cstring>

#include <zlib.h>

#include "binio.hpp"

std::string zlib_decompress(std::istream & input)
{
    z_stream strm{};
    if(auto ret = inflateInit2(&strm, 16 | MAX_WBITS); ret != Z_OK)
        throw std::runtime_error("Could not initialize ZLIB");

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
                else if(ret == Z_STREAM_END)
                {
                    decompress_finished = true;
                    break;
                }
                data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + std::size(buffer) - strm.avail_out);
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

// TODO: zlib_compress()

// Note:
// This does not aim to be a complete Minecraft NBT parser
// We are only interested in the bare minimum to extract image data from a map item file
// as such, most fields are ignored rather than read

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

    switch(tag_type)
    {
        case nbt_tag::end:
            break;
        case nbt_tag::byte:
            break;
        case nbt_tag::int16:
            break;
        case nbt_tag::int32:
            break;
        case nbt_tag::int64:
            break;
        case nbt_tag::float32:
            break;
        case nbt_tag::float64:
            break;
        case nbt_tag::byte_array:
            break;
        case nbt_tag::string:
            break;
        case nbt_tag::list:
            break;
        case nbt_tag::compound:
            break;
        case nbt_tag::int32_array:
            break;
        case nbt_tag::int64_array:
            break;
    }
}

MCMap::MCMap(std::istream & input)
{
    std::istringstream decompressed_input{zlib_decompress(input)};

}

void MCMap::write(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
}
