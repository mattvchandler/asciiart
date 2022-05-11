#include "sif.hpp"

#include <stdexcept>

void Sif::open(std::istream & input, const Args &)
{
    input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    try
    {
        set_size(25, 6);
        for(auto && row: image_data_)
            std::fill(std::begin(row), std::end(row), Color{0, 0, 0, 0});

        std::size_t y = 0, x = 0;
        for(auto i = std::istreambuf_iterator<char>{input}; i != std::istreambuf_iterator<char>{}; ++i)
        {
            switch(*i)
            {
                case '0':
                    if(image_data_[y][x].a == 0)
                        image_data_[y][x] = {0, 0, 0, 255};
                    break;
                case '1':
                    if(image_data_[y][x].a == 0)
                        image_data_[y][x] = {255, 255, 255, 255};
                    break;
                case '2':
                    break;
                default:
                    continue;
            }

            if(++x == width_)
            {
                x = 0;
                if(++y == height_)
                {
                    y = 0;
                }
            }
        }
    }
    catch(std::ios_base::failure & e)
    {
        if(input.bad())
            throw std::runtime_error{"Error reading SIF: could not read file"};
        else
            throw std::runtime_error{"Error reading SIF: unexpected end of file"};
    }
}
