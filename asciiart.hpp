#ifndef ASCIIART_HPP
#define ASCIIART_HPP

#include "args.hpp"
#include "font.hpp"
#include "codecs/image.hpp"

void write_ascii(const Image & img, const Char_vals & char_vals, const Args & args);
#endif // ASCIIART_HPP
