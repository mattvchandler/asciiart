#include "binio.hpp"

#include "config.h"

#ifdef ASCIIART_BIG_ENDIAN
#error no
static const binio_endian host_endian = binio_endian::BE;
#else
static const binio_endian host_endian = binio_endian::LE;
#endif

#ifdef HAS_ENDIAN
#include <endian.h>
#endif
#ifdef HAS_BYTESWAP
#include <byteswap.h>
#endif

#ifndef HAS_BSWAP16
std::uint16_t bswap_16(std::uint16_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap16(a);
    #elif defined(_MSC_VER)
    return _byteswap_ushort(a);
    #else
    return ((a >> 8) & 0x00FFu)
         | ((a << 8) & 0xFF00u);
    #endif
}
#endif

#ifndef HAS_BSWAP32
std::uint32_t bswap_32(std::uint32_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap32(a);
    #elif defined(_MSC_VER)
    return _byteswap_ulong(a);
    #else
    return ((a >> 24) & 0x000000FFu)
         | ((a >>  8) & 0x0000FF00u)
         | ((a <<  8) & 0x00FF0000u)
         | ((a << 24) & 0xFF000000u);
    #endif
}
#endif

#ifndef HAS_BSWAP64
std::uint64_t bswap_64(std::uint64_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap64(a);
    #elif defined(_MSC_VER)
    return _byteswap_uint64(a);
    #else
    return ((a >> 56) & 0x00000000000000FFull)
         | ((a >> 40) & 0x000000000000FF00ull)
         | ((a >> 24) & 0x0000000000FF0000ull)
         | ((a >>  8) & 0x00000000FF000000ull)
         | ((a <<  8) & 0x000000FF00000000ull)
         | ((a << 24) & 0x0000FF0000000000ull)
         | ((a << 40) & 0x00FF000000000000ull)
         | ((a << 56) & 0xFF00000000000000ull)
    #endif
}
#endif

#ifndef HAS_LE16TOH
std::uint16_t le16toh(std::uint16_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return bswap_16(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE16TOH
std::uint16_t be16toh(std::uint16_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return a;
#else
    return bswap_16(a);
#endif
}
#endif

#ifndef HAS_LE32TOH
std::uint32_t le32toh(std::uint32_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return bswap_32(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE32TOH
std::uint32_t be32toh(std::uint32_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return a;
#else
    return bswap_32(a);
#endif
}
#endif

#ifndef HAS_LE64TOH
std::uint64_t le64toh(std::uint64_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return bswap_64(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE64TOH
std::uint64_t be64toh(std::uint64_t a)
{
#ifdef ASCIIART_BIG_ENDIAN
    return a;
#else
    return bswap_64(a);
#endif
}
#endif

void readb(std::istream & i, std::uint64_t & t, binio_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le64toh(t);
        else
            t = be64toh(t);
    }
}
void readb(std::istream & i, std::int64_t & t, binio_endian endian)
{
    readb(i, reinterpret_cast<std::uint64_t&>(t), endian);
}
void readb(std::istream & i, std::uint32_t & t, binio_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le32toh(t);
        else
            t = be32toh(t);
    }
}
void readb(std::istream & i, std::int32_t & t, binio_endian endian)
{
    readb(i, reinterpret_cast<std::uint32_t&>(t), endian);
}
void readb(std::istream & i, std::uint16_t & t, binio_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le16toh(t);
        else
            t = be16toh(t);
    }
}
void readb(std::istream & i, std::int16_t & t, binio_endian endian)
{
    readb(i, reinterpret_cast<std::uint16_t&>(t), endian);
}
void readb(std::istream & i, std::uint8_t & t, binio_endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
}
void readb(std::istream & i, std::int8_t & t, binio_endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
}
void readb(std::istream & i, float & t, binio_endian endian)
{
    static_assert(sizeof(t) == 4);

    char buf[4];
    i.read(buf, sizeof(buf));

    if(host_endian != endian)
    {
        std::swap(buf[0], buf[3]);
        std::swap(buf[1], buf[2]);
    }

    t = *reinterpret_cast<float *>(buf);
}
void readb(std::istream & i, double & t, binio_endian endian)
{
    static_assert(sizeof(t) == 8);

    char buf[8];
    i.read(buf, sizeof(buf));

    if(host_endian != endian)
    {
        std::swap(buf[0], buf[7]);
        std::swap(buf[1], buf[6]);
        std::swap(buf[2], buf[5]);
        std::swap(buf[3], buf[4]);
    }

    t = *reinterpret_cast<double *>(buf);
}

std::string readstr(std::istream & i, std::size_t size)
{
    std::string s(size, ' ');
    i.read(std::data(s), size);

    return s;
}

void writeb(std::ostream & o, std::uint64_t t, binio_endian endian)
{
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le64toh(t);
        else
            t = be64toh(t);
    }
    o.write(reinterpret_cast<char*>(&t), sizeof(t));
}
void writeb(std::ostream & o, std::int64_t t, binio_endian endian)
{
    writeb(o, *reinterpret_cast<std::uint64_t*>(&t), endian);
}
void writeb(std::ostream & o, std::uint32_t t, binio_endian endian)
{
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le32toh(t);
        else
            t = be32toh(t);
    }
    o.write(reinterpret_cast<char*>(&t), sizeof(t));
}
void writeb(std::ostream & o, std::int32_t t, binio_endian endian)
{
    writeb(o, *reinterpret_cast<std::uint32_t*>(&t), endian);
}
void writeb(std::ostream & o, std::uint16_t t, binio_endian endian)
{
    if(host_endian != endian)
    {
        if(endian == binio_endian::LE)
            t = le16toh(t);
        else
            t = be16toh(t);
    }
    o.write(reinterpret_cast<char*>(&t), sizeof(t));
}
void writeb(std::ostream & o, std::int16_t t, binio_endian endian)
{
    writeb(o, *reinterpret_cast<std::uint16_t*>(&t), endian);
}
void writeb(std::ostream & o, std::uint8_t t, binio_endian)
{
    o.write(reinterpret_cast<char*>(&t), sizeof(t));
}
void writeb(std::ostream & o, std::int8_t t, binio_endian)
{
    o.write(reinterpret_cast<char*>(&t), sizeof(t));
}
void writeb(std::ostream & o, float t, binio_endian endian)
{
    static_assert(sizeof(t) == 4);

    char buf[4];

    *reinterpret_cast<float *>(buf) = t;

    if(host_endian != endian)
    {
        std::swap(buf[0], buf[3]);
        std::swap(buf[1], buf[2]);
    }

    o.write(buf, sizeof(buf));
}
void writeb(std::ostream & o, double t, binio_endian endian)
{
    static_assert(sizeof(t) == 8);

    char buf[8];

    *reinterpret_cast<float *>(buf) = t;

    if(host_endian != endian)
    {
        std::swap(buf[0], buf[7]);
        std::swap(buf[1], buf[6]);
        std::swap(buf[2], buf[5]);
        std::swap(buf[3], buf[4]);
    }

    o.write(buf, sizeof(buf));
}

void writestr(std::ostream & o, std::string_view s)
{
    o.write(std::data(s), std::size(s));
}
