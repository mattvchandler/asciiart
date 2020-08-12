# ASCIIart

Display an image in the terminal using ANSI colors, or ASCII art.

Supports a variety of image formats:

* AVIF (requires libavif)
* BMP
* BPG (requires libbpg)
* FLIF (requires libflif)
* GIF (requires giflib)
* HEIF / HEIC (requires libheif)
* ICO / CUR
* JPEG (requires libjpeg)
* JPEG 2000 (requires OpenJPEG)
* PCX
* PNG (requires libpng)
* PPM / PGM / PBM
* [SIF](https://adventofcode.com/2019/day/8)
* SVG (requires librsvg)
* TGA
* TIFF (requires libtiff)
* WebP (requires libwebp)
* XPM (requires libxpm)

Basic conversion is also supported for some formats with the `--convert` flag.
Conversion can be done from any of the above formats to one of the following
formats:

* AVIF (requires libavif)
* BMP
* FLIF (requires libflif)
* GIF (requires giflib)
* ICO / CUR
* JPEG (requires libjpeg)
* PCX
* PNG (requires libpng)
* PPM / PGM / PBM
* TGA
