#include "svg.hpp"

#include <stdexcept>

#include <librsvg/rsvg.h>
#include <gio/gio.h>

struct RAII_stack
{
    ~RAII_stack()
    {
        for(auto i = std::rbegin(objs); i != std::rend(objs); ++i)
        {
            auto [d, free_fun] = *i;
            free_fun(d);
        }
    }
    template <typename T, typename U>
    void push(T * d, void (*free_fun)(U*))
    {
        objs.emplace_back(reinterpret_cast<void*>(d), reinterpret_cast<void (*)(void*)>(free_fun));
    }

    std::vector<std::pair<void *, void (*)(void*)>> objs;
};

RsvgHandle * get_svg_handle(std::istream & input, const std::string & filename)
{
    auto data = Image::read_input_to_memory(input);

    GFile * base = g_file_new_for_path((filename == "-") ? "." : filename.c_str());
    GInputStream * is = g_memory_input_stream_new_from_data(std::data(data), std::size(data), nullptr);

    GError * err {nullptr};
    RsvgHandle * svg_handle = rsvg_handle_new_from_stream_sync(is, base, RSVG_HANDLE_FLAGS_NONE, nullptr, &err);

    g_object_unref(is);
    g_object_unref(base);
    data.clear();

    if(!svg_handle)
    {
        std::string message {err->message};
        g_error_free(err);
        throw std::runtime_error { "Error reading SVG: " + message };
    }

    return svg_handle;
}

void Svg::open(std::istream & input, const Args & args)
{
    RAII_stack rs;

    RsvgHandle * svg_handle = get_svg_handle(input, args.input_filename);
    rs.push(svg_handle, g_object_unref);

    rsvg_handle_set_dpi(svg_handle, 75.0);
    gdouble width{0.0}, height{0.0};
#if LIBRSVG_MAJOR_VERSION > 2 || LIBRSVG_MINOR_VERSION >= 52
    rsvg_handle_get_intrinsic_size_in_pixels(svg_handle, &width, &height);

#else
    RsvgDimensionData dims;
    rsvg_handle_get_dimensions(svg_handle, &dims);
    width = dims.width;
    height = dims.height;
#endif

    cairo_surface_t * bmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    rs.push(bmp, cairo_surface_destroy);
    if(cairo_surface_status(bmp) != CAIRO_STATUS_SUCCESS)
        throw std::runtime_error { "Error creating SVG cairo surface" };

    cairo_t * cr = cairo_create(bmp);
    rs.push(cr, cairo_destroy);
    if(cairo_status(cr) != CAIRO_STATUS_SUCCESS)
        throw std::runtime_error {"Error creating SVG cairo object"};

#if LIBRSVG_MAJOR_VERSION > 2 || LIBRSVG_MINOR_VERSION >= 52
    auto viewport = RsvgRectangle {.x=0.0, .y=0.0, .width=width, .height=height};
    if(GError * err = nullptr; !rsvg_handle_render_document(svg_handle, cr, &viewport, &err))
    {
        std::string message {err->message};
        g_error_free(err);
        throw std::runtime_error{"Error rendering SVG: " + message};
    }
#else
    if(!rsvg_handle_render_cairo(svg_handle, cr))
    {
        throw std::runtime_error{"Error rendering SVG"};
    }
#endif

    set_size(width, height);
    if(static_cast<std::size_t>(cairo_image_surface_get_stride(bmp)) < width_ * 4)
        throw std::runtime_error {"Invalid SVG stride"};

    for(std::size_t row = 0; row < height_; ++row)
    {
        for(std::size_t col = 0; col < width_; ++col)
        {
            std::size_t pix_i = cairo_image_surface_get_stride(bmp) * row + 4 * col;

            image_data_[row][col].b = cairo_image_surface_get_data(bmp)[pix_i];
            image_data_[row][col].g = cairo_image_surface_get_data(bmp)[pix_i + 1];
            image_data_[row][col].r = cairo_image_surface_get_data(bmp)[pix_i + 2];
            image_data_[row][col].a = cairo_image_surface_get_data(bmp)[pix_i + 3];
        }
    }
}
