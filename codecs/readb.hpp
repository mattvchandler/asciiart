#ifndef READB_HPP
#define READB_HPP

#include <istream>

#include <cstdint>

enum class readb_endian {BE, LE};
void readb(std::istream & i, std::uint32_t & t, readb_endian endian = readb_endian::LE);
void readb(std::istream & i,  std::int32_t & t, readb_endian endian = readb_endian::LE);
void readb(std::istream & i, std::uint16_t & t, readb_endian endian = readb_endian::LE);
void readb(std::istream & i,  std::int16_t & t, readb_endian endian = readb_endian::LE);
void readb(std::istream & i,  std::uint8_t & t);
void readb(std::istream & i,   std::int8_t & t);

#endif // READB_HPP
