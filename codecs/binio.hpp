#ifndef BINIO_HPP
#define BINIO_HPP

#include <array>
#include <bit>
#include <istream>
#include <string>

#include <cstdint>
#include <type_traits>

template<typename T> concept Byte_input_iter =
#ifdef _cpp_lib_concepts
std::input_iterator<T> &&
#else
// poor man's input_iterator
requires(T begin, T end)
{
    begin == end;
    ++begin;
    *begin;
    *begin++;
    requires std::is_same_v<decltype(begin == end), bool>;
    requires std::is_same_v<decltype(++begin), T&>;
} &&
#endif
requires { requires sizeof(*std::declval<T>()) == 1; };

template<typename T> concept Byte_output_iter =
#ifdef _cpp_lib_concepts
std::output_iterator<T>;
#else
// poor man's output_iterator
requires(T begin)
{
    *begin;
    ++begin;
    begin++;
    *begin++;
    requires std::is_same_v<decltype(++begin), T&>;
};
#endif

// mixed endian systems apparently do exist, so do a static_assert to make sure we're one or the other
static_assert(std::endian::native == std::endian::big || std::endian::native == std::endian::little);

template <typename T>
T bswap(T a)
{
    auto & buf = reinterpret_cast<std::byte(&)[sizeof(T)]>(a);

    if constexpr(sizeof(T) == 1)
        return a;
    else if constexpr(sizeof(T) == 2)
    {
        #if defined(__GNUC__) // also clang
        *reinterpret_cast<std::uint16_t *>(buf) = __builtin_bswap16(*reinterpret_cast<std::uint16_t *>(buf));
        #elif defined(_MSC_VER)
        *reinterpret_cast<std::uint16_t *>(buf) = _byteswap_ushort(*reinterpret_cast<std::uint16_t *>(&buf));
        #else
        std::swap(buf[0], buf[1]);
        #endif
    }
    else if constexpr(sizeof(T) == 4)
    {
        #if defined(__GNUC__) // also clang
        *reinterpret_cast<std::uint32_t *>(buf) = __builtin_bswap32(*reinterpret_cast<std::uint32_t *>(&buf));
        #elif defined(_MSC_VER)
        *reinterpret_cast<std::uint32_t *>(buf) = _byteswap_ulong(*reinterpret_cast<std::uint32_t *>(&buf));
        #else
        std::swap(buf[0], buf[3]);
        std::swap(buf[1], buf[2]);
        #endif
    }
    else if constexpr(sizeof(T) == 8)
    {
        #if defined(__GNUC__) // also clang
        *reinterpret_cast<std::uint64_t *>(buf) = __builtin_bswap64(*reinterpret_cast<std::uint64_t *>(&buf));
        #elif defined(_MSC_VER)
        *reinterpret_cast<std::uint64_t *>(buf) = _byteswap_uint64(*reinterpret_cast<std::uint64_t *>(&buf));
        #else
        std::swap(buf[0], buf[7]);
        std::swap(buf[1], buf[6]);
        std::swap(buf[2], buf[5]);
        std::swap(buf[3], buf[4]);
        #endif
    }
    else
        static_assert(sizeof(T) != 1 && sizeof(T) != 2 && sizeof(T) != 4 && sizeof(T) != 8, "unsupported type for bswap");

    return a;
}

template <typename T> requires (!std::is_enum_v<T>)
void readb(std::istream & i, T & t, std::endian endian = std::endian::little)
{
    auto & buf = reinterpret_cast<char(&)[sizeof(T)]>(t);
    i.read(buf, sizeof(buf));
    if(std::endian::native != endian)
        t = bswap(t);
}

template <typename T> requires (!std::is_enum_v<T>)
T readb(std::istream & i, std::endian endian = std::endian::little)
{
    T t{0};
    readb(i, t, endian);
    return t;
}

template <typename T, Byte_input_iter InputIter> requires (!std::is_enum_v<T>)
void readb(InputIter & begin, InputIter end, T & t, std::endian endian = std::endian::little)
{
    auto & buf = reinterpret_cast<std::byte(&)[sizeof(T)]>(t);
    for(auto && i: buf)
    {
        if(begin == end)
            throw std::runtime_error{"Unexpected end of input"};
        i = static_cast<std::byte>(*begin++);
    }
    if(std::endian::native != endian)
        t = bswap(t);
}

template <typename T, Byte_input_iter InputIter> requires (!std::is_enum_v<T>)
T readb(InputIter & begin, InputIter end, std::endian endian = std::endian::little)
{
    T t{0};
    readb(begin, end, t, endian);
    return t;
}

template<typename E> requires std::is_enum_v<E>
void readb(std::istream & i, E & e, std::endian endian = std::endian::little)
{
    readb<std::underlying_type_t<E>>(i, reinterpret_cast<std::underlying_type_t<E>&>(e), endian);
}

template<typename E> requires std::is_enum_v<E>
E readb(std::istream & i, std::endian endian = std::endian::little)
{
    return static_cast<E>(readb<std::underlying_type_t<E>>(i, endian));
}

template<typename E, Byte_input_iter InputIter> requires std::is_enum_v<E>
void readb(InputIter & begin, InputIter end, E & e, std::endian endian = std::endian::little)
{
    readb<std::underlying_type_t<E>>(begin, end, reinterpret_cast<std::underlying_type<E>&>(e), endian);
}

template<typename E, Byte_input_iter InputIter> requires std::is_enum_v<E>
E readb(InputIter & begin, InputIter end, std::endian endian = std::endian::little)
{
    return static_cast<E>(readb<std::underlying_type_t<E>>(begin, end, endian));
}

inline std::string readstr(std::istream & i, std::size_t len)
{
    std::string s(len, ' ');
    i.read(std::data(s), len);

    return s;
}

template <Byte_input_iter InputIter>
std::string readstr(InputIter & begin, InputIter end, std::size_t len)
{
    std::string s(len, '\0');
    for(std::size_t i = 0; i < len; ++i)
    {
        if(begin == end)
            throw std::runtime_error{"Unexpected end of input"};
        s[i] = static_cast<char>(*begin++);
    }
    return s;
}

template <typename T> requires(!std::is_enum_v<T>)
void writeb(std::ostream & o, T t, std::endian endian = std::endian::little)
{
    if(std::endian::native != endian)
        t = bswap(t);
    auto & buf = reinterpret_cast<char(&)[sizeof(T)]>(t);
    o.write(buf, sizeof(buf));
}

template <typename T, Byte_output_iter OutputIter> requires(!std::is_enum_v<T>)
void writeb(OutputIter & o, T t, std::endian endian = std::endian::little)
{
    if(std::endian::native != endian)
        t = bswap(t);
    auto & buf = reinterpret_cast<char(&)[sizeof(T)]>(t);
    for(auto && b: buf)
        *o++ = b;
}

template <typename E> requires(std::is_enum_v<E>)
void writeb(std::ostream & o, E e, std::endian endian = std::endian::little)
{
    writeb(o, static_cast<std::underlying_type_t<E>>(e), endian);
}

template <typename E, Byte_output_iter OutputIter> requires(std::is_enum_v<E>)
void writeb(OutputIter & o, E e, std::endian endian = std::endian::little)
{
    writeb(o, static_cast<std::underlying_type_t<E>>(e), endian);
}

inline void writestr(std::ostream & o, const std::string_view & s)
{
    o.write(std::data(s), std::size(s));
}

template <Byte_output_iter OutputIter>
inline void writestr(OutputIter & o, const std::string_view & s)
{
    for(auto && b: s)
        *o++ = b;
}

#endif // BINIO_HPP
