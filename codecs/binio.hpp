#ifndef BINIO_HPP
#define BINIO_HPP

#include <istream>

#include <cstdint>

enum class binio_endian {BE, LE};

void readb(std::istream & i, std::uint32_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::int32_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i, std::uint16_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::int16_t & t, binio_endian endian = binio_endian::LE);
void readb(std::istream & i,  std::uint8_t & t);
void readb(std::istream & i,   std::int8_t & t);

void writeb(std::ostream & o, std::uint32_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::int32_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o, std::uint16_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::int16_t t, binio_endian endian = binio_endian::LE);
void writeb(std::ostream & o,  std::uint8_t t);
void writeb(std::ostream & o,   std::int8_t t);

#endif // BINIO_HPP
