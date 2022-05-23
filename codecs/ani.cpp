#include "ani.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cstdint>

#include "binio.hpp"
#include "sub_args.hpp"
#include "ico.hpp"

void Ani::open(std::istream & input, const Args & args)
{
    constexpr auto id_size = 4u;
    constexpr auto anih_size = 36u;

    this_is_first_image_ = false;

    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        input.ignore(id_size); // RIFF

        std::uint32_t file_size;
        readb(input, file_size);

        input.ignore(id_size); // ICON
        auto file_pos = id_size;

        std::vector<Ico> frames;

        bool header_read = false;
        std::uint32_t num_frames {0};
        std::uint32_t animation_steps {0};
        std::uint32_t delay_count {0}; // in 1/60th of a second

        std::vector<std::uint32_t> rate;
        std::vector<std::uint32_t> seq;

        while(file_pos < file_size)
        {
            auto chunk_tag = readstr(input, id_size);
            file_pos += id_size;
            if(chunk_tag == "fram")
                continue;

            std::uint32_t chunk_size;
            readb(input, chunk_size);
            file_pos += sizeof(chunk_size);

            if(chunk_tag == "LIST")
            {
                // do nothing
            }
            else if(chunk_tag == "INAM" || chunk_tag == "IART")
            {
                input.ignore(chunk_size);
                file_pos += chunk_size;
            }
            else if(chunk_tag == "anih")
            {
                if(chunk_size != anih_size)
                    throw std::runtime_error{"Error reading ANI: Invalid size for ani header"};

                if(header_read)
                    throw std::runtime_error{"Error reading ANI: multiple ani headers detected"};

                input.ignore(sizeof(std::uint32_t)); // bytes in header
                readb(input, num_frames);
                readb(input, animation_steps);
                input.ignore(sizeof(std::uint32_t) * 4); // reserved
                readb(input, delay_count);
                input.ignore(sizeof(std::uint32_t)); // flags

                file_pos += anih_size;
                header_read = true;
            }
            else if(chunk_tag == "rate")
            {
                for(auto i = 0u; i < chunk_size / sizeof(std::uint32_t); ++i)
                {
                    rate.emplace_back();
                    readb(input, rate.back());
                }
                file_pos += chunk_size;
            }
            else if(chunk_tag == "seq ")
            {
                for(auto i = 0u; i < chunk_size / sizeof(std::uint32_t); ++i)
                {
                    seq.emplace_back();
                    readb(input, seq.back());
                }
                file_pos += chunk_size;
            }
            else if(chunk_tag == "icon")
            {
                auto ico = std::string(chunk_size, '\0');
                input.read(std::data(ico), chunk_size);
                auto ico_input = std::istringstream{ico};

                frames.emplace_back();
                frames.back().open(ico_input, args);
                file_pos += chunk_size;
            }
            else
            {
                throw std::runtime_error{"Error reading ANI: Unrecognized chunk: " + chunk_tag};
            }
        }

        if(std::size(frames) != num_frames)
            throw std::runtime_error{"Error reading ANI: frame count mismatched"};

        if(!std::empty(rate) && std::size(rate) != animation_steps)
            throw std::runtime_error{"Error reading ANI: rate count mismatched"};

        if(!std::empty(seq) && std::size(seq) != animation_steps)
            throw std::runtime_error{"Error reading ANI: seq count mismatched"};

        if(animation_steps < num_frames)
            throw std::runtime_error{"Error reading ANI: not enough frames"};

        if(num_frames == 0)
            throw std::runtime_error{"Error reading ANI: no frames"};

        if(std::empty(seq))
        {
            seq.resize(animation_steps);
            for(auto i = 0u, j = 0u; i < animation_steps; ++i)
            {
                seq[i] = j++;
                if(j == num_frames)
                    j = 0;
            }
        }

        if(std::empty(rate))
            rate = std::vector<std::uint32_t>(animation_steps, delay_count);

        frame_delays_.reserve(animation_steps);
        images_.reserve(animation_steps);

        for(auto i = 0u; i < animation_steps; ++i)
        {
            frame_delays_.emplace_back(rate[i] / 60.0f);
            images_.emplace_back(frames.at(seq.at(i)));
        }

        if(!args.animate && !args.image_no)
            copy_image_data(images_[0]);
    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading ANI: could not read file"};
        else
            throw std::runtime_error{"Error reading ANI: unexpected end of file"};
    }
}
