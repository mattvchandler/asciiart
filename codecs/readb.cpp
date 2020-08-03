#include "readb.hpp"

#include "config.h"

#ifdef BIG_ENDIAN
static const readb_endian host_endian = readb_endian::BE;
#else
static const readb_endian host_endian = readb_endian::LE;
#endif

#ifndef HAS_BSWAP16
std::uint16_t bswap_16(std::uint16_t a)
{
    #if defined(__GNUC__) // also clang
    return __builtin_bswap16(a);
    #elif defined(_MSC_VER)
    return _byteswap_ushort(a);
    #else
    return ((a >> 8) & 0x00FF) | ((a << 8) & 0xFF00);
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
    return ((a >> 24) & 0x000000FF) | ((a >> 8) & 0x0000FF00) | ((a << 8) & 0x00FF0000) | ((a << 24) & 0xFF000000);
    #endif
}
#endif

#ifndef HAS_LE16TOH
std::uint16_t le16toh(std::uint16_t a)
{
#ifdef BIG_ENDIAN
    return bswap_16(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE16TOH
std::uint16_t be16toh(std::uint16_t a)
{
#ifdef BIG_ENDIAN
    return a;
#else
    return bswap_16(a);
#endif
}
#endif

#ifndef HAS_LE32TOH
std::uint32_t le32toh(std::uint32_t a)
{
#ifdef BIG_ENDIAN
    return bswap_32(a);
#else
    return a;
#endif
}
#endif

#ifndef HAS_BE32TOH
std::uint32_t be32toh(std::uint32_t a)
{
#ifdef BIG_ENDIAN
    return a;
#else
    return bswap_32(a);
#endif
}
#endif

void readb(std::istream & i, std::uint32_t & t, readb_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == readb_endian::LE)
            t = le32toh(t);
        else
            t = be32toh(t);
    }
}
void readb(std::istream & i, std::int32_t & t, readb_endian endian)
{
    readb(i, reinterpret_cast<std::uint32_t&>(t), endian);
}
void readb(std::istream & i, std::uint16_t & t, readb_endian endian)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
    if(host_endian != endian)
    {
        if(endian == readb_endian::LE)
            t = le16toh(t);
        else
            t = be16toh(t);
    }
}
void readb(std::istream & i, std::int16_t & t, readb_endian endian)
{
    readb(i, reinterpret_cast<std::uint16_t&>(t), endian);
}
void readb(std::istream & i, std::uint8_t & t)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
}
void readb(std::istream & i, std::int8_t & t)
{
    i.read(reinterpret_cast<char*>(&t), sizeof(t));
}

