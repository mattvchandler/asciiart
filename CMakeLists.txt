cmake_minimum_required (VERSION 3.8 FATAL_ERROR)
project(asciiart CXX)

set(CMAKE_CXX_STANDARD 20)
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

include(CheckIncludeFiles)
if(WIN32)
    check_include_files(windows.h HAS_WINDOWS)
endif()

check_include_files(libbpg.h BPG_FOUND)
check_include_files(flif.h FLIF_ENC_FOUND)
check_include_files(flif_dec.h FLIF_DEC_FOUND)

add_executable(asciiart
    args.cpp
    display.cpp
    font.cpp
    main.cpp
    codecs/image.cpp
    codecs/sub_args.cpp
    codecs/bmp.cpp
    codecs/bmp_common.cpp
    codecs/ico.cpp
    codecs/motologo.cpp
    codecs/pcx.cpp
    codecs/pnm.cpp
    codecs/sif.cpp
    codecs/srf.cpp
    codecs/tga.cpp
    )

target_include_directories(asciiart PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

find_package(Freetype QUIET)
if(FREETYPE_FOUND)
    pkg_check_modules(FONTCONFIG fontconfig)
    if(FONTCONFIG_FOUND)
        target_include_directories(asciiart PRIVATE
            ${FREETYPE_INCLUDE_DIRS}
            ${FONTCONFIG_INCLUDE_DIRS}
            )

        target_link_libraries(asciiart
            ${FREETYPE_LIBRARIES}
            ${FONTCONFIG_LIBRARIES}
            )
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

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
