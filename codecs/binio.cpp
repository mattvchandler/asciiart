#include "binio.hpp"

#include "config.h"

#ifdef ASCIIART_BIG_ENDIAN
#error no
static const binio_endian host_endian = binio_endian::BE;
#else
static const binio_endian host_endian = binio_endian::LE;
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
            t = htole64(t);
        else
            t = htobe64(t);
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
            t = htole32(t);
        else
            t = htobe32(t);
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
            t = htole16(t);
        else
            t = htobe16(t);
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
