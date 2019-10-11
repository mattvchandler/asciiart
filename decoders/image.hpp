#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <array>
#include <istream>
#include <memory>
#include <string>

#include "../args.hpp"

unsigned char rgb_to_gray(unsigned char r, unsigned char g, unsigned char b);
float rgb_to_gray(float r, float g, float b);

// set to the size of the longest magic number
constexpr std::size_t max_header_len = 12; // 12 bytes needed to identify JPEGs

class Image
{
public:
    virtual ~Image() = default;
    virtual unsigned char get_pix(std::size_t row, std::size_t col) const = 0;
    virtual size_t get_width() const = 0;
    virtual size_t get_height() const = 0;
    using Header = std::array<char, max_header_len>;
    static bool header_cmp(unsigned char a, char b);
};

class Header_stream: public std::istream
{
public:
    Header_stream(const Image::Header & header, std::istream & input);

    template<typename T> void readb(T & t)
    {
        read(reinterpret_cast<char *>(&t), sizeof(T));
    }
    template<typename T> T readb()
    {
        T t;
        readb(t);
        return t;
    }

private:
    class Header_buf: public std::streambuf
    {
    public:
        Header_buf(const Image::Header & header, std::istream & input);

    protected:
        virtual int underflow() override;
        virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override;
        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;

    private:
        pos_type current_pos() const;
        static const std::size_t buffer_size {2048};
        std::array<char, buffer_size> buffer_;
        std::istream & input_;
        pos_type pos_{0};
    };
    Header_buf buf_;
};

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args);

#endif // IMAGE_HPP
