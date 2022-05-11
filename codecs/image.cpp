#include "image.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <tuple>

#include <cassert>
#include <cmath>
#include <cstring>

#ifdef HAS_ENDIAN
#include <endian.h>
#endif
#ifdef HAS_BYTESWAP
#include <byteswap.h>
#endif

#include "avif.hpp"
#include "bmp.hpp"
#include "bpg.hpp"
#include "flif.hpp"
#include "gif.hpp"
#include "heif.hpp"
#include "ico.hpp"
#include "jp2.hpp"
#include "jpeg.hpp"
#include "jxl.hpp"
#include "mcmap.hpp"
#include "motologo.hpp"
#include "openexr.hpp"
#include "pcx.hpp"
#include "png.hpp"
#include "pnm.hpp"
#include "sif.hpp"
#include "srf.hpp"
#include "svg.hpp"
#include "tga.hpp"
#include "tiff.hpp"
#include "webp.hpp"
#include "xpm.hpp"

const char * Early_exit::what() const noexcept { return "Success"; }

bool Image::header_cmp(unsigned char a, char b){ return a == static_cast<unsigned char>(b); };

std::vector<unsigned char> Image::read_input_to_memory(std::istream & input)
{
    // read whole stream into memory
    std::vector<unsigned char> data;
    std::array<char, 4096> buffer;
    while(input)
    {
        input.read(std::data(buffer), std::size(buffer));
        if(input.bad())
            throw std::runtime_error {"Error reading input file"};

        data.insert(std::end(data), std::begin(buffer), std::begin(buffer) + input.gcount());
    }
    return data;
}

void Image::set_size(std::size_t w, std::size_t h)
{
    width_ = w; height_ = h;
    image_data_.resize(height_);
    for(auto && row: image_data_)
        row.resize(width_);
}

void Image::transpose_image(exif::Orientation orientation)
{
    if(orientation == exif::Orientation::r_90 || orientation == exif::Orientation::r_270)
    {
        // prepare a buffer for transposed data if rotated 90 or 270 degrees
        decltype(image_data_) transpose_buf;

        transpose_buf.resize(width_);
        for(auto & row: transpose_buf)
            row.resize(height_);

        for(std::size_t row = 0; row < width_; ++row)
        {
            for(std::size_t col = 0; col < height_; ++col)
            {
                if(orientation == exif::Orientation::r_90)
                    transpose_buf[row][col] = image_data_[col][width_ - row - 1];
                else // r_270
                    transpose_buf[row][col] = image_data_[height_ - col - 1][row];
            }
        }

        std::swap(width_, height_);
        std::swap(image_data_, transpose_buf);
    }
    else if(orientation == exif::Orientation::r_180)
    {
        std::reverse(std::begin(image_data_), std::end(image_data_));
        for(auto && row: image_data_)
            std::reverse(std::begin(row), std::end(row));
    }
}

Image Image::scale(std::size_t new_width, std::size_t new_height) const
{
    Image new_img(new_width, new_height);

    const auto px_col = static_cast<float>(width_)  / static_cast<float>(new_width);
    const auto px_row = static_cast<float>(height_) / static_cast<float>(new_height);

    float row = 0.0f;
    for(std::size_t new_row = 0; new_row < new_height; ++new_row, row += px_row)
    {
        float col = 0.0f;
        for(std::size_t new_col = 0; new_col < new_width; ++new_col, col += px_col)
        {
            float r_sum = 0.0f;
            float g_sum = 0.0f;
            float b_sum = 0.0f;
            float a_sum = 0.0f;

            float cell_count {0.0f};

            for(float y = row; y < row + px_row && y < height_; y += 1.0f)
            {
                for(float x = col; x < col + px_col && x < width_; x += 1.0f)
                {
                    auto x_ind = static_cast<std::size_t>(x);
                    auto y_ind = static_cast<std::size_t>(y);
                    if(x_ind >= width_ || y_ind >= height_)
                        throw std::runtime_error{"Output coords out of range"};

                    auto pix = image_data_[y_ind][x_ind];

                    r_sum += static_cast<float>(pix.r) * static_cast<float>(pix.r);
                    g_sum += static_cast<float>(pix.g) * static_cast<float>(pix.g);
                    b_sum += static_cast<float>(pix.b) * static_cast<float>(pix.b);
                    a_sum += static_cast<float>(pix.a) * static_cast<float>(pix.a);

                    cell_count += 1.0f;
                }
            }

            new_img.image_data_[new_row][new_col] = Color{
                static_cast<unsigned char>(std::sqrt(r_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(g_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(b_sum / cell_count)),
                static_cast<unsigned char>(std::sqrt(a_sum / cell_count))
            };
        }
    }

    return new_img;
}

struct Octree_node // technically this would be a sedectree
{
    const static std::size_t max_depth {8};

    std::uint64_t r{0}, g{0}, b{0}, a{0};
    std::size_t pixel_count {0};
    std::array<std::unique_ptr<Octree_node>, 16> children {};

    Color to_color() const
    {
        return Color {static_cast<unsigned char>(r / pixel_count), static_cast<unsigned char>(g / pixel_count), static_cast<unsigned char>(b / pixel_count), static_cast<unsigned char>(a / pixel_count)};
    }

    static auto get_index(const Color & c, std::size_t depth)
    {
        return
            (((c.r >> (7 - depth)) & 0x01) << 3) |
            (((c.g >> (7 - depth)) & 0x01) << 2) |
            (((c.b >> (7 - depth)) & 0x01) << 1) |
             ((c.a >> (7 - depth)) & 0x01);
    }

    struct Sum
    {
        std::uint64_t r{0}, g{0}, b{0}, a{0};
        std::size_t pixel_count {0};
        std::size_t leaves_counted {0};
        std::vector<Octree_node *> reducible_descendants;

        Sum & operator+=(const Sum & other)
        {
            r += other.r;
            g += other.g;
            b += other.b;
            a += other.a;
            pixel_count += other.pixel_count;
            leaves_counted += other.leaves_counted;
            reducible_descendants.insert(std::end(reducible_descendants), std::begin(other.reducible_descendants), std::end(other.reducible_descendants));

            return *this;
        }
    };
    Sum sum()
    {
        if(pixel_count > 0)
            return Sum{r, g, b, a, pixel_count, 1, {}};

        Sum totals;

        totals.reducible_descendants.push_back(this);
        for(auto && i: children)
        {
            if(i)
            {
                totals += i->sum();
            }
        }

        return totals;
    }
    std::size_t reduce(const Sum & s)
    {
        assert(pixel_count == 0);     // Can't reduce leaf nodes
        assert(s.leaves_counted > 1); // Can't reduce nodes with fewer than 1 leaf

        r = s.r;
        g = s.g;
        b = s.b;
        a = s.a;
        pixel_count = s.pixel_count;

        for(auto && i: children)
        {
            if(i)
                i.reset();
        }

        return s.leaves_counted;
    }

    Octree_node * split(const Color & c, std::size_t depth)
    {
        assert(pixel_count > 0); // can only split leaves
        assert(depth < max_depth);

        auto avg = to_color();
        auto c_index = get_index(c, depth + 1);
        auto avg_index = get_index(avg, depth + 1);

        children[c_index]   = std::make_unique<Octree_node>();
        children[avg_index] = std::make_unique<Octree_node>();

        children[avg_index]->r = r;
        children[avg_index]->g = g;
        children[avg_index]->b = b;
        children[avg_index]->a = a;
        children[avg_index]->pixel_count = pixel_count;
        r = g = b = a = pixel_count = 0;

        return children[c_index].get();
    }

    void collect_colors(std::vector<Color> & palette) const
    {
        if(pixel_count > 0)
        {
            palette.emplace_back(to_color());
        }
        else
        {
            for(auto && i: children)
            {
                if(i)
                    i->collect_colors(palette);
            }
        }
    }

    Color lookup_color(const Color & c) const
    {
        auto build_color = [](Color & color, std::size_t index, std::size_t depth)
        {
            color.r |= ((index >> 3) & 0x01) << (7 - depth);
            color.g |= ((index >> 2) & 0x01) << (7 - depth);
            color.b |= ((index >> 1) & 0x01) << (7 - depth);
            color.a |= ( index       & 0x01) << (7 - depth);
        };

        Color path_color {0, 0, 0, 0};
        bool exact_match = true;

        auto node = this;
        for(std::size_t depth = 0; depth < max_depth; ++depth)
        {
            if(node->pixel_count)
                return node->to_color();

            if(exact_match)
            {
                if(auto index = get_index(c, depth); node->children[index])
                {
                    build_color(path_color, index, depth);
                    node = node->children[index].get();
                    continue;
                }
                else
                    exact_match = false;
            }

            // if we're not at a leaf, and the exact leaf is missing, find the child that represents the closest color without exceeding the value of any channel
            auto closest_index = std::numeric_limits<std::size_t>::max();
            auto closest_not_exceeding_index = std::numeric_limits<std::size_t>::max();

            auto closest_dist = std::numeric_limits<float>::max();
            auto closest_not_exceeding_dist = std::numeric_limits<float>::max();

            Color closest_node_color;
            Color closest_not_exceeding_node_color;

            for(int i = 0; i < static_cast<int>(std::size(node->children)); ++i)
            {
                if(node->children[i])
                {
                    auto node_color = path_color;

                    // append this depth's color information
                    build_color(node_color, i, depth);

                    // don't exceed the value on any channel (unless there's no other choice). The next layer down will only increase the value of each channel
                    auto not_exceeding = (node_color.r <= c.r || node_color.g <= c.g || node_color.b <= c.b || node_color.a <= c.a);

                    auto dist = color_dist2(node_color, c);
                    if(dist < closest_dist)
                    {
                        closest_index = i;
                        closest_dist = dist;
                        closest_node_color = node_color;
                    }
                    if(not_exceeding && dist < closest_not_exceeding_dist)
                    {
                        closest_not_exceeding_index = i;
                        closest_not_exceeding_dist = dist;
                        closest_not_exceeding_node_color = node_color;
                    }
                }
            }
            if(closest_index >= std::size(node->children))
                break;

            if(closest_not_exceeding_index < std::size(node->children))
            {
                node = node->children[closest_not_exceeding_index].get();
                path_color = closest_not_exceeding_node_color;
            }
            else
            {
                node = node->children[closest_index].get();
                path_color = closest_node_color;
            }
        }

        if(node->pixel_count)
            return node->to_color();
        else
            throw std::logic_error{"Color not found"};
    }
};

std::tuple<Octree_node, std::vector<Color>, bool> octree_quantitize(const Image & image, std::size_t num_colors, bool gif_transparency)
{
    if(num_colors == 0)
        throw std::domain_error {"empty palette requested"};

    unsigned char alpha_threshold = 127;

    std::vector<Color> palette;
    palette.reserve(num_colors);

    std::size_t num_leaves{0};
    bool reduced_colors {false};

    Octree_node root;

    std::array<std::set<Octree_node *>, Octree_node::max_depth> reducible_nodes;
    reducible_nodes[0].insert(&root);

    for(std::size_t row = 0; row < image.get_height(); ++row)
    {
        for(std::size_t col = 0; col < image.get_width(); ++col)
        {
            auto c = image[row][col];

            if(gif_transparency)
            {
                if(c.a > alpha_threshold)
                    c.a = 255;
                else
                    c = {0, 0, 0, 0};
            }

            Octree_node * node = &root;

            for(std::size_t i = 0; i < Octree_node::max_depth; ++i)
            {
                if(node->pixel_count) // is this a leaf node?
                {
                    // if room for more than 1 node, split a leaf
                    if(i < Octree_node::max_depth - 1 && num_leaves < num_colors)
                    {
                        reducible_nodes[i].insert(node);
                        node = node->split(c, i);
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }

                auto index = Octree_node::get_index(c, i);

                if(!node->children[index])
                {
                    node->children[index] = std::make_unique<Octree_node>();
                    if(i < Octree_node::max_depth - 1)
                        reducible_nodes[i + 1].insert(node->children[index].get());
                }

                node = node->children[index].get();
            }
            // at a leaf node

            // is this a new leaf_node?
            if(node->pixel_count == 0)
                ++num_leaves;

            node->r += c.r;
            node->g += c.g;
            node->b += c.b;
            node->a += c.a;

            ++node->pixel_count;

            while(num_leaves > num_colors)
            {
                reduced_colors = true;

                // reduce a node to get back under the limit
                // try to find a reducible node (non leaf w/ # of leaf descendants > 1)
                // * At the lowest level possible
                // * Representing the lowest # of pixels possible
                Octree_node * min_reduce_node {nullptr};
                Octree_node::Sum min_reduce_node_sum;
                min_reduce_node_sum.pixel_count = std::numeric_limits<std::size_t>::max();
                for(std::size_t i = std::size(reducible_nodes); i-- > 0;)
                {
                    if(!std::empty(reducible_nodes[i]))
                    {
                        decltype(std::begin(reducible_nodes[i])) min_reduce_node_it;

                        for(auto n = std::begin(reducible_nodes[i]); n != std::end(reducible_nodes[i]); ++n)
                        {
                            auto sum = (*n)->sum();
                            if(sum.leaves_counted > 1 && sum.pixel_count < min_reduce_node_sum.pixel_count)
                            {
                                min_reduce_node = *n;
                                min_reduce_node_sum = sum;
                                min_reduce_node_it = n;
                            }
                        }

                        if(min_reduce_node)
                        {
                            reducible_nodes[i].erase(min_reduce_node_it);

                            // remove all children nodes from reducible_nodes
                            for(std::size_t j = i + 1; j < std::size(reducible_nodes); ++j)
                            {
                                for(auto && n: min_reduce_node_sum.reducible_descendants)
                                    reducible_nodes[j].erase(n);
                            }
                            break;
                        }
                    }
                }

                if(!min_reduce_node)
                    throw std::logic_error{"Could not find a node to reduce"};

                num_leaves -= min_reduce_node->reduce(min_reduce_node_sum) - 1;
            }
        }
    }

    root.collect_colors(palette);

    assert(std::size(palette) <= num_colors);

    return {std::move(root), std::move(palette), reduced_colors};
}

std::vector<Color> Image::generate_palette(std::size_t num_colors, bool gif_transparency) const
{
    return std::move(std::get<1>(octree_quantitize(*this, num_colors, gif_transparency)));
}

std::vector<Color> Image::generate_and_apply_palette(std::size_t num_colors, bool gif_transparency)
{
    auto octree = octree_quantitize(*this, num_colors, gif_transparency);

    auto & root         = std::get<0>(octree);
    auto & palette      = std::get<1>(octree);
    auto reduced_colors = std::get<2>(octree);

    if(reduced_colors)
        dither([&root](const Color & c){ return root.lookup_color(c); });

    return std::move(palette);
}

void Image::dither(const std::function<Color(const Color &)> & palette_fun)
{
    if(height_ < 2 || width_ < 2)
        return;

    // Floyd-Steinberg dithering

    // keep a copy of the current and next row converted to floats for running calculations
    std::vector<FColor> current_row(width_), next_row(width_);
    for(std::size_t col = 0; col < width_; ++col)
    {
        next_row[col] = image_data_[0][col];
        if(next_row[col].a > 0.5f)
            next_row[col].a = 1.0f;
        else
            next_row[col] = {0.0f, 0.0f, 0.0f, 0.0f};
    }

    for(std::size_t row = 0; row < height_; ++row)
    {
        std::swap(next_row, current_row);
        if(row < height_ - 1)
        {
            for(std::size_t col = 0; col < width_; ++col)
            {
                next_row[col] = image_data_[row + 1][col];
                if(next_row[col].a > 0.5f)
                    next_row[col].a = 1.0f;
                else
                    next_row[col] = {0.0f, 0.0f, 0.0f, 0.0f};
            }
        }

        for(std::size_t col = 0; col < width_; ++col)
        {
            auto old_pix = current_row[col];
            Color new_pix = palette_fun(old_pix.clamp());

            // convert back to int and store to actual pixel data
            image_data_[row][col] = new_pix;

            auto quant_error = old_pix - new_pix;

            if(col < width_ - 1)
                current_row[col + 1] += quant_error * 7.0f / 16.0f;
            if(row < height_ - 1)
            {
                if(col > 0)
                    next_row[col - 1] += quant_error * 3.0f / 16.0f;

                next_row[col    ] += quant_error * 5.0f / 16.0f;

                if(col < width_ - 1)
                    next_row[col + 1] += quant_error * 1.0f / 16.0f;
            }
        }
    }
}

void Image::open(std::istream &, const Args &)
{
    throw std::logic_error{"Open not implemented"};
}
void Image::convert(const Args & args) const
{
    if(!args.convert_filename)
        return;

    std::ofstream out{args.convert_filename->first, std::ios_base::binary};
    if(!out)
        throw std::runtime_error {"Could not open " + args.convert_filename->first + " for writing: " + std::strerror(errno)};

    auto & ext = args.convert_filename->second;

    if(false); // dummy statement
    #ifdef AVIF_FOUND
    else if(ext == ".avif")
        Avif::write(out, *this, args.invert);
    #endif
    else if(ext == ".bmp")
        Bmp::write(out, *this, args.invert);
    else if(ext == ".cur")
        Ico::write_cur(out, *this, args.invert);
    else if(ext == ".ico")
        Ico::write_ico(out, *this, args.invert);
    #ifdef ZLIB_FOUND
    else if(ext == ".dat")
        MCMap::write(out, *this, args.bg, args.invert);
    #endif
    #ifdef FLIF_ENC_FOUND
    else if(ext == ".flif")
        Flif::write(out, *this, args.invert);
    #endif
    #ifdef GIF_FOUND
    else if(ext == ".gif")
        Gif::write(out, *this, args.invert);
    #endif
    #ifdef HEIF_FOUND
    else if(ext == ".heif")
        Heif::write(out, *this, args.invert);
    #endif
    #ifdef JPEG_FOUND
    else if(ext == ".jpeg" || ext == ".jpg")
        Jpeg::write(out, *this, args.bg, args.invert);
    #endif
    #ifdef JP2_FOUND
    else if(ext == ".jp2")
        Jp2::write(out, *this, args.invert);
    #endif
    #ifdef JXL_FOUND
    else if(ext == ".jxl")
        Jxl::write(out, *this, args.invert);
    #endif
    #ifdef OpenEXR_FOUND
    else if(ext == ".exr")
        OpenEXR::write(out, *this, args.invert);
    #endif
    else if(ext == ".pcx")
        Pcx::write(out, *this, args.bg, args.invert);
    #ifdef PNG_FOUND
    else if(ext == ".png")
        Png::write(out, *this, args.invert);
    #endif
    else if(ext == ".pbm")
        Pnm::write_pbm(out, *this, args.bg, args.invert);
    else if(ext == ".pgm")
        Pnm::write_pgm(out, *this, args.bg, args.invert);
    else if(ext == ".ppm")
        Pnm::write_ppm(out, *this, args.bg, args.invert);
    else if(ext == ".pam")
        Pnm::write_pam(out, *this, args.invert);
    else if(ext == ".pfm")
        Pnm::write_pfm(out, *this, args.bg, args.invert);
    else if(ext == ".tga")
        Tga::write(out, *this, args.invert);
    #ifdef TIFF_FOUND
    else if(ext == ".tif")
        Tiff::write(out, *this, args.invert);
    #endif
    #ifdef WEBP_FOUND
    else if(ext == ".webp")
        Webp::write(out, *this, args.invert);
    #endif
    #ifdef XPM_FOUND
    else if(ext == ".xpm")
        Xpm::write(out, *this, args.invert);
    #endif
    else
        throw std::runtime_error {"Unsupported conversion type: " + ext};
}

void Image::handle_extra_args(const Args & args)
{
    if(!std::empty(args.extra_args))
        throw std::runtime_error{args.help_text + "\nUnrecognized option '" + args.extra_args.front()};
}

bool Image::supports_multiple_images() const { return false; }
bool Image::supports_animation() const { return false; }

[[nodiscard]] std::unique_ptr<Image> get_image_data(const Args & args)
{
    std::string extension;
    std::ifstream input_file;
    if(args.input_filename != "-")
    {
        input_file.open(args.input_filename, std::ios_base::in | std::ios_base::binary);
        auto pos = args.input_filename.find_last_of('.');
        if(pos != std::string::npos)
            extension = args.input_filename.substr(pos);
        for(auto && i: extension)
            i = std::tolower(i);
    }
    std::istream & input = args.input_filename == "-" ? std::cin : input_file;

    if(!input)
        throw std::runtime_error{"Could not open input file " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

    Image::Header header;

    input.read(std::data(header), std::size(header));

    if(input.eof()) // technically, some image files could be smaller than 12 bytes, but they wouldn't be interesting images
        throw std::runtime_error{"Could not read file header " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": not enough bytes"};
    else if(!input)
        throw std::runtime_error{"Could not read input file " + (args.input_filename == "-" ? "" : ("(" + args.input_filename + ") ")) + ": " + std::string{std::strerror(errno)}};

    // rewind (seekg(0) not always supported for pipes)
    for(auto i = std::rbegin(header); i != std::rend(header); ++i)
        input.putback(*i);
    if(input.bad())
        throw std::runtime_error{"Unable to rewind stream"};

    std::unique_ptr<Image> img;
    switch(args.force_file)
    {
    case Args::Force_file::detect:
        if(is_avif(header))
        {
            #ifdef AVIF_FOUND
            img = std::make_unique<Avif>();
            #else
            throw std::runtime_error{"Not compiled with AVIF support"};
            #endif
        }
        else if(is_bmp(header))
        {
            img = std::make_unique<Bmp>();
        }
        else if(is_bpg(header))
        {
            #ifdef BPG_FOUND
            img = std::make_unique<Bpg>();
            #else
            throw std::runtime_error{"Not compiled with BPG support"};
            #endif
        }
        else if(is_flif(header))
        {
            #ifdef FLIF_DEC_FOUND
            img = std::make_unique<Flif>();
            #else
            throw std::runtime_error{"Not compiled with FLIF support"};
            #endif
        }
        else if(is_gif(header))
        {
            #ifdef GIF_FOUND
            img = std::make_unique<Gif>();
            #else
            throw std::runtime_error{"Not compiled with GIF support"};
            #endif
        }
        else if(is_heif(header))
        {
            #ifdef HEIF_FOUND
            img = std::make_unique<Heif>();
            #else
            throw std::runtime_error{"Not compiled with HEIF support"};
            #endif
        }
        else if(is_ico(header))
        {
            img = std::make_unique<Ico>();
        }
        else if(is_jp2(header))
        {
            #ifdef JP2_FOUND
            img = std::make_unique<Jp2>(Jp2::Type::JP2);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
            #endif
        }
        else if(is_jpx(header))
        {
            #ifdef JP2_FOUND
            img = std::make_unique<Jp2>(Jp2::Type::JPX);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
            #endif
        }
        else if(is_openexr(header))
        {
            #ifdef OpenEXR_FOUND
            img = std::make_unique<OpenEXR>();
            #else
            throw std::runtime_error{"Not compiled with OpenExr support"};
            #endif
        }
        else if(is_jpeg(header))
        {
            #ifdef JPEG_FOUND
            img = std::make_unique<Jpeg>();
            #else
            throw std::runtime_error{"Not compiled with JPEG support"};
            #endif
        }
        else if(is_jxl(header))
        {
            #ifdef JXL_FOUND
            img = std::make_unique<Jxl>();
            #else
            throw std::runtime_error{"Not compiled with JPEG XL support"};
            #endif
        }
        else if(is_motologo(header))
        {
            img = std::make_unique<MotoLogo>();
        }
        else if(is_png(header))
        {
            #ifdef PNG_FOUND
            img = std::make_unique<Png>();
            #else
            throw std::runtime_error{"Not compiled with PNG support"};
            #endif
        }
        else if(is_pnm(header))
        {
            img = std::make_unique<Pnm>();
        }
        else if(is_srf(header))
        {
            img = std::make_unique<Srf>();
        }
        else if(is_tiff(header))
        {
            #ifdef TIFF_FOUND
            img = std::make_unique<Tiff>();
            #else
            throw std::runtime_error{"Not compiled with TIFF support"};
            #endif
        }
        else if(is_webp(header))
        {
            #ifdef WEBP_FOUND
            img = std::make_unique<Webp>();
            #else
            throw std::runtime_error{"Not compiled with WEBP support"};
            #endif
        }
        else if(extension == ".dat")
        {
            #ifdef ZLIB_FOUND
            img = std::make_unique<MCMap>();
            #else
            throw std::runtime_error{"Not compiled with Minecraft map item / .dat support"};
            #endif
        }
        else if(extension == ".pcx")
        {
            img = std::make_unique<Pcx>();
        }
        else if(extension == ".svg" || extension == ".svgz")
        {
            #ifdef SVG_FOUND
            img = std::make_unique<Svg>();
            #else
            throw std::runtime_error{"Not compiled with SVG support"};
            #endif
        }
        else if(extension == ".tga")
        {
            img = std::make_unique<Tga>();
        }
        else if(extension == ".xpm")
        {
            #ifdef XPM_FOUND
            img = std::make_unique<Xpm>();
            #else
            throw std::runtime_error{"Not compiled with XPM support"};
            #endif
        }
        else if(extension == ".jpt")
        {
            #ifdef JP2_FOUND
            img = std::make_unique<Jp2>(Jp2::Type::JPT);
            #else
            throw std::runtime_error{"Not compiled with JPEG 2000 support"};
            #endif
        }
        else
        {
            throw std::runtime_error{"Unknown input file format"};
        }
        break;
    #ifdef ZLIB_FOUND
    case Args::Force_file::mcmap:
        img = std::make_unique<MCMap>();
        break;
    #endif
    case Args::Force_file::pcx:
        img = std::make_unique<Pcx>();
        break;
    #ifdef SVG_FOUND
    case Args::Force_file::svg:
        img = std::make_unique<Svg>();
        break;
    #endif
    case Args::Force_file::tga:
        img = std::make_unique<Tga>();
        break;
    #ifdef XPM_FOUND
    case Args::Force_file::xpm:
        img = std::make_unique<Xpm>();
        break;
    #endif
    case Args::Force_file::aoc_2019_sif:
        img = std::make_unique<Sif>();
        break;
    default:
        throw std::runtime_error{"Unhandled file format switch"};
    }

    if(!img->supports_multiple_images() && args.image_no > 0)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support multiple images"};

    if(!img->supports_animation() && args.animate)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support animation"};

    if(!img->supports_multiple_images() && args.get_image_count)
    {
        std::cout<<"0\n";
        throw Early_exit{};
    }

    img->handle_extra_args(args);
    img->open(input, args);

    return img;
}
