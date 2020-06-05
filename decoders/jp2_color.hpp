#ifndef JP2_COLOR_HPP
#define JP2_COLOR_HPP

#include <openjpeg.h>

bool color_sycc_to_rgb(opj_image_t * img);
bool color_cmyk_to_rgb(opj_image_t * image);
bool color_esycc_to_rgb(opj_image_t * image);

#endif // JP2_COLOR_HPP
