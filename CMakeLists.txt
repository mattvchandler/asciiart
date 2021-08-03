cmake_minimum_required (VERSION 3.8 FATAL_ERROR)
project(asciiart CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(cxxopts REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(libavif QUIET)
if(libavif_FOUND)
    message(STATUS "Found libavif version ${libavif_VERSION}") # TODO: is there a cleaner way to do this?
    set(AVIF_FOUND ${libavif_FOUND})
endif()
find_package(GIF QUIET)
find_package(JPEG QUIET)
find_package(PNG QUIET)
find_package(TIFF QUIET)
find_package(ZLIB QUIET)
pkg_check_modules(EXIF libexif)
pkg_check_modules(HEIF libheif)
pkg_check_modules(JP2 libopenjp2)
pkg_check_modules(JXL libjxl)
pkg_check_modules(OpenEXR OpenEXR)
pkg_check_modules(SVG librsvg-2.0)
pkg_check_modules(WEBP libwebp)
pkg_check_modules(XPM xpm)

find_package(Freetype QUIET)
if(FREETYPE_FOUND)
    pkg_check_modules(FONTCONFIG fontconfig)
endif()

include(CheckIncludeFiles)
check_include_files(unistd.h HAS_UNISTD)
check_include_files(endian.h HAS_ENDIAN)
check_include_files(byteswap.h HAS_BYTESWAP)

check_include_files(libbpg.h BPG_FOUND)
check_include_files(flif.h FLIF_ENC_FOUND)
check_include_files(flif_dec.h FLIF_DEC_FOUND)

include(CheckSymbolExists)
check_symbol_exists(le64toh "endian.h" HAS_LE64TOH)
check_symbol_exists(le32toh "endian.h" HAS_LE32TOH)
check_symbol_exists(le16toh "endian.h" HAS_LE16TOH)
check_symbol_exists(be64toh "endian.h" HAS_BE64TOH)
check_symbol_exists(be32toh "endian.h" HAS_BE32TOH)
check_symbol_exists(be16toh "endian.h" HAS_BE16TOH)
check_symbol_exists(htole64 "endian.h" HAS_HTOLE64)
check_symbol_exists(htole32 "endian.h" HAS_HTOLE32)
check_symbol_exists(htole16 "endian.h" HAS_HTOLE16)
check_symbol_exists(htobe64 "endian.h" HAS_HTOBE64)
check_symbol_exists(htobe32 "endian.h" HAS_HTOBE32)
check_symbol_exists(htobe16 "endian.h" HAS_HTOBE16)
check_symbol_exists(bswap_64 "byteswap.h" HAS_BSWAP64)
check_symbol_exists(bswap_32 "byteswap.h" HAS_BSWAP32)
check_symbol_exists(bswap_16 "byteswap.h" HAS_BSWAP16)

include(TestBigEndian)
test_big_endian(ASCIIART_BIG_ENDIAN)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_executable(asciiart
    args.cpp
    display.cpp
    font.cpp
    main.cpp
    codecs/image.cpp
    codecs/binio.cpp
    codecs/bmp.cpp
    codecs/bmp_common.cpp
    codecs/ico.cpp
    codecs/pcx.cpp
    codecs/pnm.cpp
    codecs/sif.cpp
    codecs/srf.cpp
    codecs/tga.cpp
    )

target_include_directories(asciiart PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(asciiart PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    )

target_link_libraries(asciiart
    ${FREETYPE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    )

if(AVIF_FOUND)
    target_sources(asciiart PRIVATE codecs/avif.cpp)
    target_link_libraries(asciiart avif)
endif()
if(BPG_FOUND)
    target_sources(asciiart PRIVATE codecs/bpg.cpp)
    target_link_libraries(asciiart bpg)
endif()
if(EXIF_FOUND)
    target_sources(asciiart PRIVATE codecs/exif.cpp)
    target_include_directories(asciiart PRIVATE ${EXIF_INCLUDE_DIRS})
    target_link_libraries(asciiart ${EXIF_LIBRARIES})
endif()
if(FLIF_ENC_FOUND OR FLIF_DEC_FOUND)
    target_sources(asciiart PRIVATE codecs/flif.cpp)
    if(FLIF_ENC_FOUND)
        target_link_libraries(asciiart flif)
    else()
        target_link_libraries(asciiart flif_dec)
    endif()
endif()
if(GIF_FOUND)
    target_sources(asciiart PRIVATE codecs/gif.cpp)
    target_include_directories(asciiart PRIVATE ${GIF_INCLUDE_DIRS})
    target_link_libraries(asciiart ${GIF_LIBRARIES})
endif()
if(HEIF_FOUND)
    target_sources(asciiart PRIVATE codecs/heif.cpp)
    target_include_directories(asciiart PRIVATE ${HEIF_INCLUDE_DIRS})
    target_link_libraries(asciiart ${HEIF_LIBRARIES})
endif()
if(JP2_FOUND)
    target_sources(asciiart PRIVATE codecs/jp2.cpp codecs/jp2_color.cpp)
    target_include_directories(asciiart PRIVATE ${JP2_INCLUDE_DIRS})
    target_link_libraries(asciiart ${JP2_LIBRARIES})
endif()
if(JPEG_FOUND)
    target_sources(asciiart PRIVATE codecs/jpeg.cpp)
    target_include_directories(asciiart PRIVATE ${JPEG_INCLUDE_DIRS})
    target_link_libraries(asciiart ${JPEG_LIBRARIES})
endif()
if(JXL_FOUND)
    target_sources(asciiart PRIVATE codecs/jxl.cpp)
    target_include_directories(asciiart PRIVATE ${JXL_INCLUDE_DIRS})
    target_link_libraries(asciiart ${JXL_LIBRARIES})
endif()
if(OpenEXR_FOUND)
    target_sources(asciiart PRIVATE codecs/openexr.cpp)
    target_include_directories(asciiart SYSTEM PRIVATE ${OpenEXR_INCLUDE_DIRS}) # SYSTEM to suppress a bunch of warnings
    target_link_libraries(asciiart ${OpenEXR_LIBRARIES})
endif()
if(PNG_FOUND)
    target_sources(asciiart PRIVATE codecs/png.cpp)
    target_include_directories(asciiart PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(asciiart ${PNG_LIBRARIES})
endif()
if(SVG_FOUND)
    target_sources(asciiart PRIVATE codecs/svg.cpp)
    target_include_directories(asciiart PRIVATE ${SVG_INCLUDE_DIRS})
    target_link_libraries(asciiart ${SVG_LIBRARIES})
endif()
if(TIFF_FOUND)
    target_sources(asciiart PRIVATE codecs/tiff.cpp)
    target_include_directories(asciiart PRIVATE ${TIFF_INCLUDE_DIRS})
    target_link_libraries(asciiart ${TIFF_LIBRARIES})
endif()
if(WEBP_FOUND)
    target_sources(asciiart PRIVATE codecs/webp.cpp)
    target_include_directories(asciiart PRIVATE ${WEBP_INCLUDE_DIRS})
    target_link_libraries(asciiart ${WEBP_LIBRARIES})
endif()
if(XPM_FOUND)
    target_sources(asciiart PRIVATE codecs/xpm.cpp)
    target_include_directories(asciiart PRIVATE ${XPM_INCLUDE_DIRS})
    target_link_libraries(asciiart ${XPM_LIBRARIES})
endif()
if(ZLIB_FOUND)
    target_sources(asciiart PRIVATE codecs/mcmap.cpp)
    target_include_directories(asciiart PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(asciiart ${ZLIB_LIBRARIES})
endif()
