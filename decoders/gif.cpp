#include "gif.hpp"

#include <iostream>

#include <gif_lib.h>

int read_fn(GifFileType* gif_file, GifByteType * data, int length) noexcept
{
    auto in = static_cast<std::istream *>(gif_file->UserData);
    if(!in)
    {
        std::cerr<<"FATAL ERROR: Could not get GIF input stream\n";
        return GIF_ERROR;
    }

    in->read(reinterpret_cast<char *>(data), length);
    if(in->bad())
    {
        std::cerr<<"FATAL ERROR: Could not read GIF image\n";
        return GIF_ERROR;
    }

    return in->gcount();
}

Gif::Gif(std::istream & input)
{
    int error_code = GIF_OK;
    GifFileType * gif = DGifOpen(&input, read_fn, &error_code);
    if(!gif)
        throw std::runtime_error{"Error setting up GIF: " + std::string{GifErrorString(error_code)}};

    if(DGifSlurp(gif) != GIF_OK)
    {
        DGifCloseFile(gif, NULL);
        throw std::runtime_error{"Error reading GIF: " + std::string{GifErrorString(gif->Error)}};
    }

    auto pal = gif->SavedImages[0].ImageDesc.ColorMap;
    if(!pal)
    {
        pal = gif->SColorMap;
        if(!pal)
        {
            DGifCloseFile(gif, NULL);
            throw std::runtime_error{"Could not find color map"};
        }
    }

    int transparency_ind = -1;
    GraphicsControlBlock gcb;
    if(DGifSavedExtensionToGCB(gif, 0, &gcb) == GIF_OK)
        transparency_ind = gcb.TransparentColor;

    set_size(gif->SWidth, gif->SHeight);

    if(gif->SavedImages[0].ImageDesc.Left != 0 || gif->SavedImages[0].ImageDesc.Top != 0
            || static_cast<std::size_t>(gif->SavedImages[0].ImageDesc.Width) != width_
            || static_cast<std::size_t>(gif->SavedImages[0].ImageDesc.Height) != height_)
    {
        throw std::runtime_error{"GIF has wrong size or offset"};
    }

    const auto & im = gif->SavedImages[0].RasterBits;

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            auto index = im[row * width_ + col];

            if(index == transparency_ind)
            {
                image_data_[row][col] = Color{};
            }
            else
            {
                auto & pal_color = pal->Colors[index];
                image_data_[row][col] = Color{pal_color.Red, pal_color.Green, pal_color.Blue};
            }
        }
    }

    DGifCloseFile(gif, NULL);
}
