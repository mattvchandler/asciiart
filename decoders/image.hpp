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

class Image
{
public:
    virtual ~Image() = default;

    unsigned char get_pix(std::size_t row, std::size_t col) const
    {
        return image_data_[row][col];
    }
    std::size_t get_width() const { return width_; }
    std::size_t get_height() const { return height_; }

    using Header = std::array<char, max_header_len>;
    static bool header_cmp(unsigned char a, char b);

protected:
    void set_size(std::size_t w, std::size_t h);

    std::size_t width_{0};
    std::size_t height_{0};
    std::vector<std::vector<unsigned char>> image_data_;
};

template <typename T>
void readb(std::istream & i, T & t)
{
    i.read(reinterpret_cast<char *>(&t), sizeof(T));
}

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args);

#endif // IMAGE_HPP
