#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <memory>
#include <string>

class Image
{
public:
    virtual ~Image() = default;
    virtual unsigned char get_pix(std::size_t row, std::size_t col) const = 0;
    virtual size_t get_width() const = 0;
    virtual size_t get_height() const = 0;
    using Header = std::array<char, 12>;
};

[[nodiscard]] std::unique_ptr<Image> get_image_data(std::string & input_filename, int bg);

#endif // IMAGE_HPP
