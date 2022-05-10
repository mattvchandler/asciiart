#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "args.hpp"
#include "codecs/image.hpp"

void display_image(const Image & img, const Args & args);
void print_image(const Image & img, const Args & args, std::ostream & out);

#endif // DISPLAY_HPP
