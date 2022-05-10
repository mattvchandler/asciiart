#include "gif.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

#include <cstdlib>
#include <cstring>

#include <gif_lib.h>

#include "../display.hpp"
#include "sub_args.hpp"

// TODO: animation support?
// giflib is a very poorly designed library. Its documentation is even worse

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

#if defined(HAS_SELECT) && defined(HAS_SIGNAL)
volatile sig_atomic_t stop_flag = 0;
volatile sig_atomic_t suspend_flag = 0;

void handle_stop(int)    { stop_flag    = 1; }
void handle_suspend(int) { suspend_flag = 1; }
#endif

Gif::Gif(std::istream & input, const Args & args)
{
    handle_extra_args(args);
    int error_code = GIF_OK;
    GifFileType * gif = DGifOpen(&input, read_fn, &error_code);
    if(!gif)
        throw std::runtime_error{"Error setting up GIF: " + std::string{GifErrorString(error_code)}};

    if(DGifSlurp(gif) != GIF_OK)
    {
        DGifCloseFile(gif, NULL);
        throw std::runtime_error{"Error reading GIF: " + std::string{GifErrorString(gif->Error)}};
    }

    if(count_)
    {
        std::cout<<gif->ImageCount<<'\n';
        throw Early_exit{};
    }

    if(frame_ >= static_cast<decltype(frame_)>(gif->ImageCount))
        throw std::runtime_error{"Error reading GIF: frame " + std::to_string(frame_) + " is out of range (0-" + std::to_string(gif->ImageCount - 1) + ")"};

    set_size(gif->SWidth, gif->SHeight);

    // default all pixels to transparent
    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            image_data_[row][col] = Color{0, 0, 0, 0};
        }
    }

    bool do_loop = animate_ && loop_;
    if(animate_)
    {
        frame_ = gif->ImageCount;
    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
        set_signal(SIGINT,   handle_stop);
        set_signal(SIGTERM,  handle_stop);
        set_signal(SIGTSTP,  handle_suspend);
    #endif
        open_alternate_buffer();
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    do
    {
        if(animate_)
            clear_buffer();

        for(auto f = composed_ ? 0u : frame_; f < frame_; ++f)
        {
            // if(animate_)
            // {
                auto frame_start = std::chrono::high_resolution_clock::now();
            // }

            auto pal = gif->SavedImages[f].ImageDesc.ColorMap;
            if(!pal)
            {
                pal = gif->SColorMap;
                if(!pal)
                {
                    DGifCloseFile(gif, NULL);
                    if(animate_)
                    {
                        close_alternate_buffer();
                    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
                        reset_signal(SIGINT);
                        reset_signal(SIGTERM);
                        reset_signal(SIGTSTP);
                    #endif
                    }
                    throw std::runtime_error{"Could not find color map"};
                }
            }

            int transparency_ind = -1;
            float framerate = 0.0f;
            GraphicsControlBlock gcb;
            if(DGifSavedExtensionToGCB(gif, f, &gcb) == GIF_OK)
            {
                transparency_ind = gcb.TransparentColor;
                framerate = 1.0f / (0.01f * gcb.DelayTime);
            }

            auto left = gif->SavedImages[f].ImageDesc.Left;
            auto top = gif->SavedImages[f].ImageDesc.Top;
            auto sub_width = static_cast<std::size_t>(gif->SavedImages[f].ImageDesc.Width);
            auto sub_height = static_cast<std::size_t>(gif->SavedImages[f].ImageDesc.Height);

            if(left + sub_width > width_ || top + sub_height > height_)
            {
                if(animate_)
                {
                    DGifCloseFile(gif, NULL);
                    close_alternate_buffer();
                #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
                    reset_signal(SIGINT);
                    reset_signal(SIGTERM);
                    reset_signal(SIGTSTP);
                #endif
                }
                throw std::runtime_error{"GIF has wrong size or offset"};
            }

            const auto & im = gif->SavedImages[f].RasterBits;

            for(std::size_t row = 0; row < sub_height; ++row)
            {
                for(std::size_t col = 0; col < sub_width; ++col)
                {
                    auto index = im[row * sub_width + col];

                    if(index != transparency_ind)
                    {
                        auto & pal_color = pal->Colors[index];
                        image_data_[row + top][col + left] = Color{pal_color.Red, pal_color.Green, pal_color.Blue};
                    }
                }
            }

            if(animate_)
            {
                reset_cursor_pos();
                display_image(*this, args);
                std::cout.flush();

                std::cout<<"suspend_flag: "<<suspend_flag<<" stop: "<<stop_flag<<'\n';

            #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
                if(suspend_flag)
                {
                    close_alternate_buffer();
                    reset_signal(SIGTSTP);
                    raise(SIGTSTP);

                    set_signal(SIGTSTP,  handle_suspend);
                    open_alternate_buffer();
                    suspend_flag = 0;
                }

                if(stop_flag)
                {
                    do_loop = false;
                    break;
                }
            #endif

                std::cout<<"F: "<<framerate<<' '<<framerate_<<'\n';
                auto frame_end = std::chrono::high_resolution_clock::now();
                auto frame_time = frame_end-frame_start;
                auto framerate_inv = std::chrono::duration_cast<decltype(frame_time)>(std::chrono::duration<float>{1.0f / (framerate_ == 0.0f ? framerate : framerate_)});
                auto sleep_time = std::max(decltype(frame_time)::zero(), framerate_inv - (frame_time));

                std::this_thread::sleep_for(sleep_time);
            }
        }
    } while(do_loop);

    if(animate_)
    {
        close_alternate_buffer();
    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
        reset_signal(SIGINT);
        reset_signal(SIGTERM);
        reset_signal(SIGTSTP);
    #endif
    }

    DGifCloseFile(gif, NULL);
}

void Gif::handle_extra_args(const Args & args)
{
    if(!std::empty(args.extra_args))
    {
        auto options = Sub_args{"GIF"};
        try
        {
            options.add_options()
                ("animate", "display animated view of image", cxxopts::value<float>()->implicit_value("0"), "FRAMERATE")
                ("loop", "loop if animating")
                ("framecount", "get a count of GIF frames")
                ("frame", "frame to extract (0-based count)", cxxopts::value<unsigned int>()->default_value("0"), "FRAME_NO")
                ("not-composed", "Show only information for the given frame, not those leading up to it");

            auto sub_args = options.parse(args.extra_args);

            animate_ = sub_args.count("animate");

            if(animate_)
                framerate_ = sub_args["animate"].as<float>();

            loop_ = sub_args.count("loop");

            count_ = sub_args.count("framecount");
            composed_ = !sub_args.count("not-composed");

            if(sub_args.count("frame"))
                frame_ = sub_args["frame"].as<unsigned int>();

            if(animate_ && (!composed_ || frame_ != 0))
            {
                throw std::runtime_error{options.help(args.help_text) + "\nCan't specify --frame or --not-composed with --animate"};
            }
        }
        catch(const cxxopts::OptionException & e)
        {
            throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
        }
    }
}

int write_fn(GifFileType * gif_file, const GifByteType * data, int length)
{
    auto out = static_cast<std::ostream *>(gif_file->UserData);
    if(!out)
    {
        std::cerr<<"FATAL ERROR: Could not get GIF output stream\n";
        return GIF_ERROR;
    }

    out->write(reinterpret_cast<const char *>(data), length);
    if(out->bad())
    {
        std::cerr<<"FATAL ERROR: Could not write GIF image\n";
        return GIF_ERROR;
    }

    return length;
}
void Gif::write(std::ostream & out, const Image & img, bool invert)
{
    int error_code = GIF_OK;
    GifFileType * gif = EGifOpen(&out, write_fn, &error_code);
    if(!gif)
        throw std::runtime_error{"Error setting up GIF: " + std::string{GifErrorString(error_code)}};

    Image img_copy{img.get_width(), img.get_height()};

    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            img_copy[row][col] = img[row][col];
            if(invert)
                img_copy[row][col].invert();
        }
    }

    auto palette = img_copy.generate_and_apply_palette(256, true);

    std::map<Color, std::size_t> color_lookup;
    std::vector<GifColorType> gif_palette(std::size(palette));

    for(std::size_t i = 0; i < std::size(palette); ++i)
    {
        auto & c = palette[i];

        color_lookup[c] = i;
        gif_palette[i] = GifColorType{ c.r, c.g, c.b };;
    }

    // Gif generation doesn't seem to work when the # of colors in the palette is < 256, so pad it to that size
    if(auto old_size = std::size(gif_palette); old_size < 256)
    {
        gif_palette.resize(256);
        std::fill(std::begin(gif_palette) + old_size, std::end(gif_palette), GifColorType{0, 0, 0});
    }

    auto gif_color_map = GifMakeMapObject(std::size(gif_palette), std::data(gif_palette));
    gif_palette.clear();

    gif->SWidth = img_copy.get_width();
    gif->SHeight = img_copy.get_height();
    gif->SColorResolution = 8;
    gif->SBackGroundColor = 0;
    gif->SColorMap = gif_color_map;

    auto gif_img = GifMakeSavedImage(gif, nullptr);
    if(!gif_img)
    {
        EGifCloseFile(gif, nullptr);
        throw std::runtime_error{"Error allocating GIF Image structure"};
    }

    gif_img->ImageDesc.Left = 0;
    gif_img->ImageDesc.Top = 0;
    gif_img->ImageDesc.Width = img_copy.get_width();
    gif_img->ImageDesc.Height = img_copy.get_height();
    gif_img->ImageDesc.Interlace = false;
    gif_img->ImageDesc.ColorMap = nullptr;
    gif_img->ExtensionBlockCount = 0;
    gif_img->ExtensionBlocks = nullptr;
    gif_img->RasterBits = reinterpret_cast<GifByteType*>(malloc(img_copy.get_width() * img_copy.get_height()));
    if(!gif_img->RasterBits)
    {
        EGifCloseFile(gif, nullptr);
        GifFreeMapObject(gif_color_map);
        throw std::runtime_error{"Error allocating GIF image data"};
    }

    for(std::size_t row = 0; row < img_copy.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img_copy.get_width(); ++col)
        {
            try
            {
                gif_img->RasterBits[row * img_copy.get_width() + col] = color_lookup.at(img_copy[row][col]);
            }
            catch(const std::out_of_range &)
            {
                GifFreeSavedImages(gif);
                EGifCloseFile(gif, nullptr);
                GifFreeMapObject(gif_color_map);
                throw std::runtime_error{"Error finding GIF color in palette"};
            }
        }
    }

    // TODO: build hash table
    auto transparent_color = std::find_if(std::begin(palette), std::end(palette), [](const Color & c) { return c.a == 0; });
    if(transparent_color != std::end(palette))
    {
        GraphicsControlBlock gcb;
        memset(&gcb, 0, sizeof(gcb));
        gcb.TransparentColor = std::distance(std::begin(palette), transparent_color);
        if(auto error_code = EGifGCBToSavedExtension(&gcb, gif, 0); error_code != GIF_OK)
        {
            GifFreeSavedImages(gif);
            EGifCloseFile(gif, nullptr);
            GifFreeMapObject(gif_color_map);
            throw std::runtime_error{"Error setting GIF transparency index: " + std::string{GifErrorString(error_code)}};
        }
    }

    // Prevent a memory leak here - EGifCloseFile / EGifSpew don't free SavedImages
    // save a pointer to them and clean it up afterwards

    auto saved_images = gif->SavedImages;

    if(auto error_code = EGifSpew(gif); error_code != GIF_OK)
    {
        GifFreeSavedImages(gif);
        EGifCloseFile(gif, nullptr);
        GifFreeMapObject(gif_color_map);
        throw std::runtime_error{"Error writing GIF to file"};
    }

    if(saved_images)
    {
        if(saved_images[0].ExtensionBlocks)
        {
            free(saved_images[0].ExtensionBlocks[0].Bytes);
            free(saved_images[0].ExtensionBlocks);
        }
        free(saved_images[0].RasterBits);
        free(saved_images);
    }

    GifFreeMapObject(gif_color_map);
}
