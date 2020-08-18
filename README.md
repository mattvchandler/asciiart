# ASCIIart

Display an image in the terminal using ANSI colors, or ASCII art.

Supports a variety of image formats:

* AVIF (requires libavif)
* BMP
* CUR / ICO
* BPG (requires libbpg)
* FLIF (requires libflif)
* GIF (requires giflib)
* HEIF / HEIC (requires libheif)
* JPEG (requires libjpeg)
* JPEG 2000 (requires OpenJPEG)
* PCX
* PNG (requires libpng)
* PPM / PGM / PBM / PAM / PFM
* [SIF](https://adventofcode.com/2019/day/8)
* SVG (requires librsvg)
* TGA
* TIFF (requires libtiff)
* WebP (requires libwebp)
* XPM (requires libxpm)

Basic conversion is also supported for some formats with the `--convert` flag.
Converted images are always as close to 32bit RGBA as supported by the format.
Conversion can be done from any of the above formats to one of the following
formats:

* AVIF (requires libavif)
* BMP
* CUR / ICO
* FLIF (requires libflif)
* GIF (requires giflib)
* HEIF / HEIC (requires libheif)
* JPEG (requires libjpeg)
* JPEG 2000 (requires OpenJPEG)
* PCX
* PNG (requires libpng)
* PPM / PGM / PBM / PAM / PFM
* TGA
* TIFF (requires libtiff)
* WebP (requires libwebp)
* XPM (requires libxpm)

### TODO:

Add support for

* JPEG XL (R/W?)
* OpenEXR (R/W)
