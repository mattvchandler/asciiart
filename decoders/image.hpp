#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "../args.hpp"

unsigned char rgb_to_gray(unsigned char r, unsigned char g, unsigned char b);
float rgb_to_gray_float(float r, float g, float b);
unsigned char rgba_to_gray(unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned char bg);
unsigned char ga_blend(unsigned char g, unsigned char a, unsigned char bg);

// set to the size of the longest magic number
constexpr std::size_t max_header_len = 12; // 12 bytes needed to identify JPEGs

struct Color
{
    unsigned char r{0}, g{0}, b{0}, a{0xFF};
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xFF): r{r}, g{g}, b{b}, a{a} {}
    explicit Color(unsigned char y): r{y}, g{y}, b{y} {}
    Color(){}
};

class Image
{
public:
    virtual ~Image() = default;

    const std::vector<Color> & operator[](std::size_t i) const
    {
        return image_data_[i];
    }
    std::size_t get_width() const { return width_; }
    std::size_t get_height() const { return height_; }

    using Header = std::array<char, max_header_len>;
    static bool header_cmp(unsigned char a, char b);

protected:
    void set_size(std::size_t w, std::size_t h);

    std::size_t width_{0};
    std::size_t height_{0};
    std::vector<std::vector<Color>> image_data_;
};

template <typename T>
void readb(std::istream & i, T & t)
{
    i.read(reinterpret_cast<char *>(&t), sizeof(T));
}

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args);

#endif // IMAGE_HPP
