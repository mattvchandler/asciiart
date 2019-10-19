#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "../args.hpp"

// set to the size of the longest magic number
constexpr std::size_t max_header_len = 12; // 12 bytes needed to identify JPEGs

struct Color
{
    unsigned char r{0}, g{0}, b{0}, a{0xFF};
    constexpr Color(){}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xFF): r{r}, g{g}, b{b}, a{a} {}
    constexpr explicit Color(unsigned char y): r{y}, g{y}, b{y} {}

    constexpr bool operator<(const Color & other) const
    {
        if(r < other.r)
            return true;
        else if(g < other.g)
            return true;
        else if(b < other.b)
            return true;
        else if(a < other.a)
            return true;
        else
            return false;
    }
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
