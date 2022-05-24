# ASCIIart

Display an image in the terminal using ANSI colors, or ASCII art.

Supports a variety of image formats:

* ANI
* AVIF (requires libavif)
* BMP
* CUR / ICO
* BPG (requires libbpg)
* FLIF (requires libflif)
* GIF (requires giflib)
* HEIF / HEIC (requires libheif)
* JPEG / MPF / MPO (requires libjpeg)
* JPEG 2000 (requires OpenJPEG)
* JPEG XL (requires libjxl)
* Minecraft map items (requires zlib)
* MNG / JNG (requires libmng)
* Moto logo.bin files
* OpenEXR (requires libopenexr)
* PCX
* PNG / APNG (requires libpng)
* PPM / PGM / PBM / PAM / PFM
* [SIF](https://adventofcode.com/2019/day/8)
* SRF (Garmin GPS vehicle icon file)
* SVG (requires librsvg)
* TGA
* TIFF (requires libtiff)
* WebP (requires libwebp)
* XPM (requires libxpm)

Basic conversion is also supported for most formats with the `--convert` flag.
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
* JPEG XL (requires libjxl)
* Minecraft map items (requires zlib)
* OpenEXR (requires libopenexr)
* PCX
* PNG (requires libpng)
* PPM / PGM / PBM / PAM / PFM
* TGA
* TIFF (requires libtiff)
* WebP (requires libwebp)
* XPM (requires libxpm)

### TODO:

Add support for

* JPEG XR? XS? XT? LS? HDR?
