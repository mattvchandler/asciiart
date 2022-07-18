#ifndef BITSTREAM_HPP
#define BITSTREAM_HPP

#include "binio.hpp"

template <Byte_input_iter InputIter>
class Input_bitstream
{
public:
    explicit Input_bitstream(InputIter in):
        in_{in}
    {}

    template <typename T>
    T read(std::uint8_t bits)
    {
        T ret{0};
        for(std::uint8_t i = 0u; i < bits; ++i)
        {
            if(bits_available_ == 0u)
            {
                bits_available_ = 8u;
                read_ = *in_++;
            }
            ret <<= 1u;
            ret |= (read_ >> --bits_available_) & 0x01u;
        }
        return ret;
    }

    std::uint8_t operator()(std::uint8_t bits)
    {
        return read<std::uint8_t>(bits);
    }

private:
    InputIter in_;
    std::uint8_t read_{0u};
    std::uint8_t bits_available_{0u};
};

template <Byte_output_iter OutputIter>
class Output_bitstream
{
public:
    explicit Output_bitstream(OutputIter out):
        out_{out}
    {}

    ~Output_bitstream()
    {
        flush_current_byte();
    }

    Output_bitstream(const Output_bitstream &) = default;
    Output_bitstream & operator=(const Output_bitstream &) = default;
    Output_bitstream(Output_bitstream &&) = default;
    Output_bitstream & operator=(Output_bitstream &&) = default;

    template <typename T>
    void write(const T & t, std::uint8_t bits)
    {
        for(auto i = bits; i-- > 0u;)
        {
            if(bits_available_ == 0u)
            {
                bits_available_ = 8u;
                *out_++ = written_;
                written_ = 0u;
            }
            written_ <<= 1;
            written_ |= (t >> i) & 0x01u;
            --bits_available_;
        }
    }

    template <typename T>
    void operator()(const T & t, std::uint8_t bits)
    {
        write(t, bits);
    }

    void flush_current_byte()
    {
        if(bits_available_ < 8u)
        {
            written_ <<= bits_available_;
            *out_++ = written_;
            bits_available_ = 8u;
        }
    }

private:
    OutputIter out_;
    std::uint8_t written_{0u};
    std::uint8_t bits_available_{8u};
};
#endif // BITSTREAM_HPP
