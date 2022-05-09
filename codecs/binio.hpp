#ifndef BINIO_HPP
#define BINIO_HPP

#include <array>
#include <bit>
#include <istream>
#include <string>

#include <cstdint>
#include <type_traits>

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

template<typename E> requires std::is_enum_v<E>
void readb(std::istream & i, E & e, std::endian endian = std::endian::little)
{
    readb<std::underlying_type_t<E>>(i, reinterpret_cast<std::underlying_type_t<E>&>(e), endian);
}

inline std::string readstr(std::istream & i, std::size_t len)
{
    std::string s(len, ' ');
    i.read(std::data(s), len);

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

template <typename E> requires(std::is_enum_v<E>)
void writeb(std::ostream & o, E e, std::endian endian = std::endian::little)
{
    writeb(o, static_cast<std::underlying_type_t<E>>(e), endian);
}

inline void writestr(std::ostream & o, const std::string_view & s)
{
    o.write(std::data(s), std::size(s));
}

#endif // BINIO_HPP
