#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <chrono>
#include <functional>
#include <istream>
#include <memory>
#include <vector>

#include "../args.hpp"
#include "../color.hpp"
#include "exif.hpp"

struct Early_exit: public std::exception
{
    const char * what() const noexcept;
};

// set to the size of the longest magic number
constexpr std::size_t max_header_len = 12; // 12 bytes needed to identify JPEGs

class Image
{
public:
    Image() = default;
    Image(std::size_t w, std::size_t h) { set_size(w, h); }

    virtual ~Image() = default;
    Image(const Image &) = default;
    Image & operator=(const Image &) = default;
    Image(Image &&) = default;
    Image & operator=(Image &&) = default;

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
    void set_size(std::size_t w, std::size_t h);

    using Header = std::array<char, max_header_len>;
    static bool header_cmp(unsigned char a, char b);
    static std::vector<unsigned char> read_input_to_memory(std::istream & input);

    Image scale(std::size_t new_width, std::size_t new_height) const;
    void transpose_image(exif::Orientation orientation);

    std::vector<Color> generate_palette(std::size_t num_colors, bool gif_transparency = false) const;
    std::vector<Color> generate_and_apply_palette(std::size_t num_colors, bool gif_transparency = false);
    void dither(const std::function<Color(const Color &)> & palette_fun);
    template <typename Iter> void dither(Iter palette_start, Iter palette_end);

    virtual void open(std::istream & input, const Args & args);
    void convert(const Args & args) const;

    virtual void handle_extra_args(const Args & args);
    virtual bool supports_multiple_images() const;
    virtual bool supports_animation() const;

    std::size_t num_images() const;
    const Image & get_image(std::size_t image_no) const;
    std::chrono::duration<float> get_frame_delay(std::size_t image_no) const;
    std::chrono::duration<float> get_default_frame_delay() const;

    char * row_buffer(std::size_t row);
    const char * row_buffer(std::size_t row) const;

protected:

    std::size_t width_{0};
    std::size_t height_{0};
    std::vector<std::vector<Color>> image_data_;

    bool this_is_first_image_ {true};
    std::vector<Image> images_;
    std::vector<std::chrono::duration<float>> frame_delays_;
    std::chrono::duration<float> default_frame_delay_ {std::chrono::milliseconds{25}};
};

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args);

template <typename Iter>
void Image::dither(Iter palette_start, Iter palette_end)
{
    dither([palette_start, palette_end](const Color & c)
    {
        return *std::min_element(palette_start, palette_end, [c](const Color & a, const Color & b)
        {
            return color_dist2(a, c) < color_dist2(b, c);
        });
    });
}

#endif // IMAGE_HPP
