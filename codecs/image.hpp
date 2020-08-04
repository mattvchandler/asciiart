#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <istream>
#include <memory>
#include <vector>

#include "../args.hpp"
#include "../color.hpp"
#include "exif.hpp"

// set to the size of the longest magic number
constexpr std::size_t max_header_len = 12; // 12 bytes needed to identify JPEGs

class Image
{
public:
    virtual ~Image() = default;

    void swap(Image & other)
    {
        std::swap(width_, other.width_);
        std::swap(height_, other.height_);
        std::swap(image_data_, other.image_data_);
    }

    const std::vector<Color> & operator[](std::size_t i) const
    {
        return image_data_[i];
    }
    std::vector<Color> & operator[](std::size_t i)
    {
        return image_data_[i];
    }
    std::size_t get_width() const { return width_; }
    std::size_t get_height() const { return height_; }

    using Header = std::array<char, max_header_len>;
    static bool header_cmp(unsigned char a, char b);
    static std::vector<unsigned char> read_input_to_memory(std::istream & input);

    Image scale(std::size_t new_width, std::size_t new_height) const;

    void convert(const Args & args) const;

    template <typename Iter>
    void dither(Iter palette_start, Iter palette_end)
    {
        if(height_ < 2 || width_ < 2)
            return;

        // Floyd-Steinberg dithering

        // keep a copy of the current and next row converted to floats for running calculations
        std::vector<FColor> current_row(width_), next_row(width_);
        for(std::size_t col = 0; col < width_; ++col)
            next_row[col] = image_data_[0][col];

        for(std::size_t row = 0; row < height_; ++row)
        {
            std::swap(next_row, current_row);
            if(row < height_ - 1)
            {
                for(std::size_t col = 0; col < width_; ++col)
                    next_row[col] = image_data_[row + 1][col];
            }

            for(std::size_t col = 0; col < width_; ++col)
            {
                auto old_pix = current_row[col];
                Color new_pix = *find_closest_palette_color(palette_start, palette_end, old_pix.clamp());

                // convert back to int and store to actual pixel data
                image_data_[row][col] = new_pix;

                auto quant_error = old_pix - new_pix;

                if(col < width_ - 1)
                    current_row[col + 1] += quant_error * 7.0f / 16.0f;
                if(row < height_ - 1)
                {
                    if(col > 0)
                        next_row[col - 1] += quant_error * 3.0f / 16.0f;

                    next_row[col    ] += quant_error * 5.0f / 16.0f;

                    if(col < width_ - 1)
                        next_row[col + 1] += quant_error * 1.0f / 16.0f;
                }
            }
        }
    }

protected:
    void set_size(std::size_t w, std::size_t h);
    void transpose_image(exif::Orientation orientation);

    std::size_t width_{0};
    std::size_t height_{0};
    std::vector<std::vector<Color>> image_data_;
};

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args);

#endif // IMAGE_HPP
