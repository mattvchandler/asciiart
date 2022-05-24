#include "motologo.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <cstdint>

#include "binio.hpp"
#include "sub_args.hpp"

/* File format notes
*
* Header:
*     magic: 9 byte str: 'MotoLogo\0'
*     header_size: LE uint32_t
*     32 byte entries (count: (header_size - 13) / 32)
*         name: 24 byte null-terminated str name
*         offset: LE uint32_t (from file start)
*         size: LE uint32_t
*
* images:
*     aligned to 512 bytes, padded with \xff
*     magic: 8 byte str: 'MotoRun\0'
*     width: BE uint16_t
*     height: BE uint16_t
*     pixel_data: RLE compressed BGR (count: size - 16)
*
* RLE compression scheme:
*     count: BE uint16_t
*         R000CCCC CCCCCCCC
*         C: pixel count
*         R: repeat
*
*     R = count & 0x8000
*     C = count & 0x0FFF
*     if(R)
*         next 3 bytes (BGR) repeated C times
*     else
*         next C * 3 bytes are raw (BGR) pixels
*
*     repeat count seems to be limited to current line
*/

constexpr auto magic_size = 9u;
constexpr auto dir_entry_size = 32u;
constexpr auto name_size = 24u;
constexpr auto image_magic_size = 8u;

void MotoLogo::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        std::size_t pos {0};

        // header
        input.ignore(magic_size); // Magic
        std::uint32_t directory_size;
        readb(input, directory_size, std::endian::little);

        pos += magic_size + sizeof(directory_size);

        const auto num_images = (directory_size - magic_size - sizeof(directory_size)) / dir_entry_size;
        auto name_lookup = std::unordered_map<std::string, unsigned int>{};
        auto locations = std::vector <std::pair<std::uint32_t, std::uint32_t>>{};

        for(auto i = 0u; i < num_images; ++i)
        {
            std::uint32_t offset, size;
            auto name = readstr(input, name_size);
            readb(input, offset, std::endian::little);
            readb(input, size, std::endian::little);

            pos += dir_entry_size;

            name.resize(name.find_first_of('\0'));
            name_lookup[name] = i;
            locations.emplace_back(offset, size);

            if(list_)
            {
                std::cout<<"  "<<name<<'\n';
            }
        }

        if(list_)
            throw Early_exit{};

        for(auto i = 0u; i < num_images; ++i)
        {
            auto [offset, size] = locations[i];
            std::cout<<"from "<<pos<<" to "<<offset<<" size "<<size<<'\n';
            input.ignore(offset - pos);
            pos = offset;

            // read image
            constexpr auto expected_image_magic = std::array{'M', 'o', 't', 'o', 'R', 'u', 'n', '\0'};
            auto image_magic = std::array<char, image_magic_size>{};
            input.read(std::data(image_magic), std::size(image_magic)); pos += std::size(image_magic);
            for(auto j = 0u; j < image_magic_size; ++j)
            {
                if(image_magic[j] != expected_image_magic[j])
                    throw std::runtime_error{"Error reading MotoLogo: Bad magic number on Image"};
            }

            std::uint16_t width, height;
            readb(input, width, std::endian::big);
            readb(input, height, std::endian::big);
            pos += sizeof(std::uint16_t) * 2;

            auto & img = images_.emplace_back(width, height);

            auto row = 0u;
            auto write_pix = [&row, col = 0u, &img](std::uint8_t r, std::uint8_t g, std::uint8_t b) mutable
            {
                if(row >= img.get_height())
                    throw std::runtime_error{"MotoLogo image data out of range"};

                auto & p = img[row][col];
                p.r = r;
                p.g = g;
                p.b = b;
                p.a = 255u;

                if(++col == img.get_width())
                {
                    col = 0u;
                    ++row;
                }
            };

            auto img_pos = 0u;
            auto check_pos = [&pos, &img_pos, size=size](unsigned int bytes = 1) { if(pos += bytes, img_pos += bytes > size) throw std::runtime_error{"MotoLogo image read past size"}; };
            while(row < img.get_height())
            {
                std::uint16_t count;
                readb(input, count, std::endian::big); check_pos(sizeof(std::uint16_t));
                if(count & 0x7000u)
                    throw std::runtime_error{"Error reading MotoLogo: bad RLE count"};

                bool repeat = count & 0x8000u;
                count &= 0x0FFFu;

                std::uint8_t r{0}, g{0}, b{0};
                if(repeat)
                {
                    readb(input, b); check_pos();
                    readb(input, g); check_pos();
                    readb(input, r); check_pos();

                    for(auto i = 0u; i < count; ++i)
                        write_pix(r, g, b);
                }
                else
                {
                    for(auto i = 0u; i < count; ++i)
                    {
                        readb(input, b); check_pos();
                        readb(input, g); check_pos();
                        readb(input, r); check_pos();
                        write_pix(r, g, b);
                    }
                }
            }
        }
        if(image_name_)
        {
            if(auto target = name_lookup.find(*image_name_); target != std::end(name_lookup))
                copy_image_data(images_[target->second]);
            else
                throw std::runtime_error{"Error reading MotoLogo: requested image '" + *image_name_ + "' not found in MotoLogo file"};
        }
        else
        {
            move_image_data(images_.front());
            images_.erase(std::begin(images_));
        }
    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading MotoLogo: could not read file"};
        else
            throw std::runtime_error{"Error reading MotoLogo: unexpected end of file"};
    }
}

void MotoLogo::handle_extra_args(const Args & args)
{
    auto options = Sub_args{"MotoLogo"};
    try
    {
        options.add_options()
            ("list-images", "list all image names contained in input file")
            ("image-name", "image name to extract", cxxopts::value<std::string>(), "IMAGE_NAME");

        auto sub_args = options.parse(args.extra_args);

        list_ = sub_args.count("list-images");

        if(sub_args.count("image-name"))
        {
            if(args.image_no)
                throw std::runtime_error{options.help(args.help_text) + "\nCan't specify --image-name with --image-no"};

            image_name_ = sub_args["image-name"].as<std::string>();
        }

        if(args.animate && args.image_no)
            throw std::runtime_error{options.help(args.help_text) + "\nCan't specify --image-no with --animate"};
    }
    catch(const cxxopts::OptionException & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}
