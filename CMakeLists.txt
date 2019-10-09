cmake_minimum_required (VERSION 3.8 FATAL_ERROR)
project(asciiart CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(Freetype REQUIRED)
find_package(cxxopts REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(PNG)
find_package(JPEG)
find_package(GIF)
pkg_check_modules(FONTCONFIG fontconfig REQUIRED)
pkg_check_modules(XPM xpm)

add_executable(asciiart
    args.cpp
    asciiart.cpp
    font.cpp
    decoders/image.cpp
    decoders/bmp.cpp
    decoders/pnm.cpp
    )

target_include_directories(asciiart PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    )

target_link_libraries(asciiart
    ${FREETYPE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    )

if(PNG_FOUND)
    target_sources(asciiart PRIVATE decoders/png.cpp)
    target_compile_definitions(asciiart PRIVATE "-DHAS_PNG")
    target_include_directories(asciiart PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(asciiart ${PNG_LIBRARIES})
endif()
if(JPEG_FOUND)
    target_sources(asciiart PRIVATE decoders/jpeg.cpp)
    target_compile_definitions(asciiart PRIVATE "-DHAS_JPEG")
    target_include_directories(asciiart PRIVATE ${JPEG_INCLUDE_DIRS})
    target_link_libraries(asciiart ${JPEG_LIBRARIES})
endif()
if(GIF_FOUND)
    target_sources(asciiart PRIVATE decoders/gif.cpp)
    target_compile_definitions(asciiart PRIVATE "-DHAS_GIF")
    target_include_directories(asciiart PRIVATE ${GIF_INCLUDE_DIRS})
    target_link_libraries(asciiart ${GIF_LIBRARIES})
endif()
if(XPM_FOUND)
    target_sources(asciiart PRIVATE decoders/xpm.cpp)
    target_compile_definitions(asciiart PRIVATE "-DHAS_XPM")
    target_include_directories(asciiart PRIVATE ${XPM_INCLUDE_DIRS})
    target_link_libraries(asciiart ${XPM_LIBRARIES})
endif()
