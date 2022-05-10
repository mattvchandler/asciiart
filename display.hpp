#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "config.h"
#ifdef HAS_SIGNAL
#include <signal.h>
#endif

#include "args.hpp"
#include "codecs/image.hpp"

void display_image(const Image & img, const Args & args);

void set_signal(int sig, void(*handler)(int));
void reset_signal(int sig);
void open_alternate_buffer();
void close_alternate_buffer();
void clear_buffer();
void reset_cursor_pos();

#endif // DISPLAY_HPP
