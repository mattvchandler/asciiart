#ifndef BINIO_HPP
#define BINIO_HPP

#include <istream>
#include <string>

#include <cstdint>

enum class binio_endian {BE, LE};

void readb(std::istream & i, std::uint64_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::int64_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i, std::uint32_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::int32_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i, std::uint16_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::int16_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::uint8_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,   std::int8_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,         float & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,        double & t, binio_endian endian = binio_endian::LE);

template<typename E, typename std::enable_if_t<std::is_enum_v<E>, int> = 0>
void readb(std::istream & i, E & t, binio_endian endian = binio_endian::LE)
{
    readb(i, reinterpret_cast<std::underlying_type_t<E>&>(t), endian);
}

std::string readstr(std::istream & i, std::size_t size);

void writeb(std::ostream & o, std::uint64_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::int64_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o, std::uint32_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::int32_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o, std::uint16_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::int16_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::uint8_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,   std::int8_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,           float, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,          double, binio_endian endian = binio_endian::LE);

template<typename E, typename std::enable_if_t<std::is_enum_v<E>, int> = 0>
void writeb(std::ostream & i, E t, binio_endian endian = binio_endian::LE)
{
    writeb(i, static_cast<std::underlying_type_t<E>>(t), endian);
}

void writestr(std::ostream & o, std::string_view s);

#endif // BINIO_HPP
