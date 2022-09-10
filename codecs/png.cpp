#include "png.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include <csetjmp>
#include <cstdint>

#include <png.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

#include "sub_args.hpp"

enum class Dispose_op: std::uint8_t {NONE = 0u, BACKGROUND = 1u, PREVIOUS = 2u};
enum class Blend_op:   std::uint8_t {SOURCE = 0u, OVER = 1u};

struct Libpng
{
    enum class Type {READ, WRITE} type;

    png_structp png_ptr{nullptr};
    png_infop info_ptr{nullptr};
    inline static std::string error_msg = {"Generic error"};

    explicit Libpng(Type t):
        type{t}
    {
        if(type == Type::READ)
            png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        else
            png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

        if(!png_ptr)
            throw std::runtime_error{"Error initializing libpng"};

        info_ptr = png_create_info_struct(png_ptr);
        if(!info_ptr)
        {
            if(type == Type::READ)
                png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            else
                png_destroy_write_struct(&png_ptr, nullptr);
            throw std::runtime_error{"Error initializing libpng info"};
        }
    }
    ~Libpng()
    {
        if(png_ptr && info_ptr)
        {
            if(type == Type::READ)
                png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            else
                png_destroy_write_struct(&png_ptr, &info_ptr);
        }
    }

    Libpng(const Libpng &) = delete;
    Libpng & operator=(const Libpng &) = delete;

    Libpng(Libpng && other):
        png_ptr{std::move(other.png_ptr)},
        info_ptr{std::move(other.info_ptr)}
    {
        other.png_ptr = nullptr;
        other.info_ptr = nullptr;
    }

    Libpng & operator=(Libpng && other)
    {
        if(this != &other)
        {
            std::swap(png_ptr , other.png_ptr);
            std::swap(info_ptr, other.info_ptr);
        }
        return *this;
    }

    void set_error_point(const std::string & msg)
    {
        error_msg = msg;
        if(setjmp(png_jmpbuf(png_ptr)))
            throw std::runtime_error{error_msg};
    }

    operator png_structp() { return png_ptr; }
    operator png_struct const *() const { return png_ptr; }

    operator png_infop() { return info_ptr; }
    operator png_info const *() const { return info_ptr; }
};
struct Animation_info
{
    Image * img;
    const Args & args;

    bool is_apng {false};
    unsigned int num_frames {0};
    unsigned int num_plays  {0};

    std::vector<png_byte> copied_chunks;
#ifdef EXIF_FOUND
    exif::Orientation orientation { exif::Orientation::r_0};
#endif

    // current frame data
    bool include_default_image {true};

    struct Frame_chunk
    {
        unsigned int   seq_no     {0};
        unsigned int   width      {0};
        unsigned int   height     {0};
        unsigned int   x_offset   {0};
        unsigned int   y_offset   {0};
        unsigned short delay_num  {0};
        unsigned short delay_den  {0};
        Dispose_op     dispose_op {Dispose_op::NONE};
        Blend_op       blend_op   {Blend_op::SOURCE};
        std::vector<png_byte> fdat;
    };

    std::vector<Frame_chunk> frame_chunks;

    Animation_info(Image * img, const Args & args):
        img{img},
        args{args}
    {}
};

void Png::open(std::istream & input, const Args & args)
{
    auto info_callback = [](png_structp png_ptr, png_infop info_ptr)
    {
        auto animation_info = reinterpret_cast<Animation_info *>(png_get_progressive_ptr(png_ptr));

        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type, compression_type, filter_type;
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_type);

        animation_info->img->set_size(width, height);

        // set transformations to convert to 32-bit RGBA
        if(!(color_type & PNG_COLOR_MASK_COLOR))
            png_set_gray_to_rgb(png_ptr);

        if(color_type & PNG_COLOR_MASK_PALETTE)
            png_set_palette_to_rgb(png_ptr);

        if(!(color_type & PNG_COLOR_MASK_ALPHA))
            png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

        if(bit_depth == 16)
            png_set_strip_16(png_ptr);

        if(bit_depth < 8)
            png_set_packing(png_ptr);

        png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        if((animation_info->args.animate || animation_info->args.image_no) && std::empty(animation_info->copied_chunks))
        {
            animation_info->copied_chunks = std::vector<png_byte> {
                137, 'P', 'N', 'G', 13, 10, 26, 10, // PNG magic number
                0, 0, 0, 13, 'I', 'H', 'D', 'R', // IHDR chunk start (13 byte payload)
            };

            auto ihdr_offset = std::size(animation_info->copied_chunks);
            animation_info->copied_chunks.resize(ihdr_offset + 13 + 4); // add space for IHDR payload and CRC
            auto ihdr = &animation_info->copied_chunks[ihdr_offset];

            png_save_uint_32(ihdr, width);
            png_save_uint_32(ihdr + 4, height);
            ihdr[8] = bit_depth;
            ihdr[9] = color_type;
            ihdr[10] = compression_type;
            ihdr[11] = filter_type;
            ihdr[12] = interlace_type;
            png_save_uint_32(ihdr + 13, 0); // garbage CRC

            if(double gamma; png_get_gAMA(png_ptr, info_ptr, &gamma))
            {
                auto gama_offset = std::size(animation_info->copied_chunks);
                animation_info->copied_chunks.resize(gama_offset + 16);
                auto gama = &animation_info->copied_chunks[gama_offset];

                png_save_int_32(gama, 4);
                auto gama_tag = std::string_view{"gAMA"};
                std::copy(std::begin(gama_tag), std::end(gama_tag), gama + 4);

                png_save_int_32(gama + 8, static_cast<int>(gamma * 100000.0));
                png_save_int_32(gama + 12, 0); // garbage CRC
            }

            if(color_type == PNG_COLOR_TYPE_PALETTE)
            {
                png_colorp palette;
                int palette_size = 0;
                png_get_PLTE(png_ptr, info_ptr, &palette, &palette_size);
                palette_size *= 3;

                auto plte_offset = std::size(animation_info->copied_chunks);
                animation_info->copied_chunks.resize(plte_offset + 12 + palette_size);
                auto plte = &animation_info->copied_chunks[plte_offset];

                png_save_uint_32(plte, palette_size);
                auto plte_tag = std::string_view{"PLTE"};
                std::copy(std::begin(plte_tag), std::end(plte_tag), plte + 4);
                std::copy(reinterpret_cast<png_bytep>(palette), reinterpret_cast<png_bytep>(palette) + palette_size, plte + 8);
                png_save_uint_32(ihdr + 12 + palette_size, 0); // garbage CRC
            }

            if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            {
                png_bytep trans_alpha;
                int num_trans = 0;
                png_color_16p trans_color;
                png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);

                auto trns_offset = std::size(animation_info->copied_chunks);
                animation_info->copied_chunks.resize(trns_offset + 12 + (color_type == PNG_COLOR_TYPE_PALETTE ? 1 : 2) * num_trans);
                auto trns = &animation_info->copied_chunks[trns_offset];

                png_save_uint_32(trns, num_trans * 2);
                auto trns_tag = std::string_view{"tRNS"};
                std::copy(std::begin(trns_tag), std::end(trns_tag), trns + 4);

                if(color_type == PNG_COLOR_TYPE_RGB)
                {
                    png_save_uint_16(trns + 8, trans_color->red);
                    png_save_uint_16(trns + 10, trans_color->green);
                    png_save_uint_16(trns + 12, trans_color->blue);
                }
                else if(color_type == PNG_COLOR_TYPE_GRAY)
                {
                    png_save_uint_16(trns + 8, trans_color->gray);
                }
                else if(color_type == PNG_COLOR_TYPE_PALETTE)
                {
                    std::copy(trans_alpha, trans_alpha + num_trans, trns + 8);
                }
                png_save_uint_32(ihdr + 12 + (color_type == PNG_COLOR_TYPE_PALETTE ? 1 : 2) * num_trans, 0); // garbage CRC
            }
        }
    };

    auto row_callback = [](png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int)
    {
        auto animation_info = reinterpret_cast<Animation_info *>(png_get_progressive_ptr(png_ptr));
        png_progressive_combine_row(png_ptr, reinterpret_cast<png_bytep>(animation_info->img->row_buffer(row_num)), new_row);
    };

    auto chunk_callback = [](png_structp png_ptr, png_unknown_chunkp chunk) -> int
    {
        auto animation_info = reinterpret_cast<Animation_info *>(png_get_progressive_ptr(png_ptr));

        auto chunk_name = std::string_view{reinterpret_cast<const char *>(chunk->name), 4};
        auto data = chunk->data;

        if(chunk_name == "eXIf")
        {
        #ifdef EXIF_FOUND
            if (chunk->size > 1)
            {
                std::vector<unsigned char> exif_buf(chunk->size + 6);
                for(auto i = 0; i < 6; ++i)
                    exif_buf[i] = "Exif\0\0"[i];
                std::copy(data, data + chunk->size, std::begin(exif_buf) + 6);

                animation_info->orientation = exif::get_orientation(std::data(exif_buf), std::size(exif_buf));
            }
            return 1;
        #else
            return 0;
        #endif
        }
        else if(!animation_info->args.animate && !animation_info->args.image_no)
            return 1;
        else if(chunk_name == "acTL") // animation control
        {
            animation_info->is_apng = true;

            animation_info->num_frames = png_get_uint_32(data); data += sizeof(std::uint32_t);
            animation_info->num_plays  = png_get_uint_32(data); data += sizeof(std::uint32_t);

            return 1;
        }
        else if(chunk_name == "fcTL") // frame control
        {
            if(!(chunk->location & PNG_AFTER_IDAT))
                animation_info->include_default_image = true;

            auto & fc = animation_info->frame_chunks.emplace_back();
            fc.seq_no = png_get_uint_32(data); data += sizeof(std::uint32_t);

            fc.width      = png_get_uint_32(data); data += sizeof(std::uint32_t);
            fc.height     = png_get_uint_32(data); data += sizeof(std::uint32_t);
            fc.x_offset   = png_get_uint_32(data); data += sizeof(std::uint32_t);
            fc.y_offset   = png_get_uint_32(data); data += sizeof(std::uint32_t);
            fc.delay_num  = png_get_uint_16(data); data += sizeof(std::uint16_t);
            fc.delay_den  = png_get_uint_16(data); data += sizeof(std::uint16_t);
            fc.dispose_op = static_cast<Dispose_op>(*data++);
            fc.blend_op   = static_cast<Blend_op>(*data++);

            return 1;
        }
        else if(chunk_name == "fdAT") // frame data
        {
            auto & fc = animation_info->frame_chunks.emplace_back();
            fc.seq_no = png_get_uint_32(data); data += sizeof(std::uint32_t);

            fc.fdat.insert(std::end(fc.fdat), data, data + chunk->size - sizeof(std::uint32_t));
            return 1;
        }

        return 0;
    };

    auto animation_info = Animation_info{this, args};

    constexpr auto self_handled_chunks = std::array<png_byte, 20>
        {'e', 'X', 'I', 'f', '\0',
         'a', 'c', 'T', 'L', '\0',
         'f', 'c', 'T', 'L', '\0',
         'f', 'd', 'A', 'T', '\0'};

    std::array<char, 4096> io_buffer;

    auto libpng = std::make_unique<Libpng>(Libpng::Type::READ);
    libpng->set_error_point("Error reading with libpng");

    png_set_progressive_read_fn(*libpng, &animation_info, info_callback, row_callback, nullptr);
    png_set_read_user_chunk_fn(*libpng, &animation_info, chunk_callback);
    png_set_keep_unknown_chunks(*libpng, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
    png_set_keep_unknown_chunks(*libpng, PNG_HANDLE_CHUNK_ALWAYS, std::data(self_handled_chunks), std::size(self_handled_chunks) / 5);

    while(input)
    {
        input.read(std::data(io_buffer), std::size(io_buffer));
        if(input.bad())
            throw std::runtime_error {"Error reading PNG file"};

        png_process_data(*libpng, *libpng, reinterpret_cast<png_bytep>(std::data(io_buffer)), input.gcount());
    }
    libpng.reset();

#ifdef EXIF_FOUND
    transpose_image(animation_info.orientation);
#endif

    if(animation_info.is_apng && (args.animate || args.image_no))
    {
        this_is_first_image_ = false;

        std::sort(std::begin(animation_info.frame_chunks), std::end(animation_info.frame_chunks), [](auto && a, auto && b){ return a.seq_no < b.seq_no; });
        Animation_info::Frame_chunk frame_ctrl;

        auto calc_delay = [](const Animation_info::Frame_chunk & fc)
        {
            if(fc.delay_den == 0)
                return std::chrono::duration<float>{static_cast<float>(fc.delay_num) / 100.0f};
            else
                return std::chrono::duration<float>{static_cast<float>(fc.delay_num) / static_cast<float>(fc.delay_den)};
        };

        // NOTE: this gets a little confusing - We are decoding each frame's image data into this->image_data_, then composing them into output_buffer, and copying each completed frames to images_
        images_.resize(animation_info.num_frames);
        frame_delays_.resize(animation_info.num_frames);

        auto output_buffer = Image{width_, height_};

        // APNG spec calls for starting with transparent black
        for(std::size_t row = 0; row < get_height(); ++row)
        {
            for(std::size_t col = 0; col < get_width(); ++col)
                output_buffer[row][col] = Color{0u, 0u, 0u, 0u};
        }

        auto frame_no = 0u;

        for(auto i = 0u; i < std::size(animation_info.frame_chunks); ++i)
        {
            auto & fc = animation_info.frame_chunks[i];

            // scan for gaps/non seq
            if(fc.seq_no != i)
                throw std::runtime_error {"Error reading APNG file: missing chunks"};

            if(std::empty(fc.fdat))
            {
                if(i == 0 && animation_info.include_default_image)
                {
                    images_[frame_no] = output_buffer = *this;
                    frame_delays_[frame_no] = calc_delay(fc);
                }
                else
                {
                    if(libpng)
                    {
                        constexpr auto iend = std::array<png_byte, 12> {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0};
                        libpng->set_error_point("Error processing APNG frame IEND");

                        png_process_data(*libpng, *libpng, const_cast<png_bytep>(std::data(iend)), std::size(iend));
                        libpng.reset();

                        transpose_image(animation_info.orientation);

                        if(composed_)
                        {
                            // the framee's data is in this->image_data_ now, so blend or replace into output_buffer
                            for(std::size_t row = 0; row < get_height(); ++row)
                            {
                                for(std::size_t col = 0; col < get_width(); ++col)
                                {
                                    if(frame_ctrl.blend_op == Blend_op::OVER)
                                    {
                                        auto bg = FColor{output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset]};
                                        auto fg = FColor{image_data_[row][col]};

                                        auto out = FColor{};
                                        out.a = fg.a + bg.a * (1.0f - fg.a);
                                        out.r = (fg.r * fg.a + bg.r * bg.a * (1.0f - fg.a)) / out.a;
                                        out.g = (fg.g * fg.a + bg.g * bg.a * (1.0f - fg.a)) / out.a;
                                        out.b = (fg.b * fg.a + bg.b * bg.a * (1.0f - fg.a)) / out.a;

                                        output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = out;
                                    }
                                    else
                                    {
                                        output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = image_data_[row][col];
                                    }
                                }
                            }

                            images_[frame_no] = output_buffer;

                            // dispose of this frame's data if requested
                            if(frame_ctrl.dispose_op == Dispose_op::BACKGROUND)
                            {
                                // clear to tranparent black
                                for(std::size_t row = 0; row < get_height(); ++row)
                                {
                                    for(std::size_t col = 0; col < get_width(); ++col)
                                        output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = Color{0u, 0u, 0u, 0u};
                                }
                            }
                            else if(frame_ctrl.dispose_op == Dispose_op::PREVIOUS)
                            {
                                // clear to previous frame
                                for(std::size_t row = 0; row < get_height(); ++row)
                                {
                                    for(std::size_t col = 0; col < get_width(); ++col)
                                    {
                                        if(frame_no > 0)
                                            output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = Color{0u, 0u, 0u, 0u};
                                        else
                                            output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = images_[frame_no - 1][row + frame_ctrl.y_offset][col + frame_ctrl.x_offset];
                                    }
                                }
                            }
                        }
                        else // not-composed
                        {
                            for(std::size_t row = 0; row < output_buffer.get_height(); ++row)
                            {
                                for(std::size_t col = 0; col < output_buffer.get_width(); ++col)
                                    output_buffer[row][col] = Color{0u, 0u, 0u, 0u};
                            }
                            for(std::size_t row = 0; row < get_height(); ++row)
                            {
                                for(std::size_t col = 0; col < get_width(); ++col)
                                    output_buffer[row + frame_ctrl.y_offset][col + frame_ctrl.x_offset] = image_data_[row][col];
                            }
                            images_[frame_no] = output_buffer;
                        }
                    }
                    frame_ctrl = fc;

                    if(fc.width == 0 || fc.height == 0 ||
                            fc.x_offset + fc.width > output_buffer.get_width() ||
                            fc.y_offset + fc.height > output_buffer.get_height())
                    {
                        throw std::runtime_error{"Error reading APNG: Invalid frame dimensions\n"};
                    }

                    frame_delays_[frame_no++] = calc_delay(fc);

                    libpng = std::make_unique<Libpng>(Libpng::Type::READ);
                    libpng->set_error_point("Error decoding APNG frame header");

                    png_set_progressive_read_fn(*libpng, &animation_info, info_callback, row_callback, nullptr);
                    png_set_read_user_chunk_fn(*libpng, &animation_info, chunk_callback);
                    png_set_keep_unknown_chunks(*libpng, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
                    png_set_crc_action(*libpng, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE); // we're going to be feeding this garbage CRC values, so tell libpng to ignore them

                    png_save_int_32(std::data(animation_info.copied_chunks) + 16, fc.width);
                    png_save_int_32(std::data(animation_info.copied_chunks) + 20, fc.height);
                    png_process_data(*libpng, *libpng, std::data(animation_info.copied_chunks), std::size(animation_info.copied_chunks));
                }
            }
            else
            {
                auto scratch_buffer = std::array<png_byte, 4>{};
                auto idat_tag = std::array<png_byte, 4>{'I', 'D', 'A', 'T'};

                png_save_int_32(std::data(scratch_buffer), std::size(fc.fdat));

                libpng->set_error_point("Error decoding APNG frame");
                png_process_data(*libpng, *libpng, std::data(scratch_buffer), std::size(scratch_buffer));
                png_process_data(*libpng, *libpng, std::data(idat_tag), std::size(idat_tag));
                png_process_data(*libpng, *libpng, std::data(fc.fdat), std::size(fc.fdat));
                png_process_data(*libpng, *libpng, std::data(scratch_buffer), std::size(scratch_buffer)); // garbage CRC
            }
        }
        libpng.reset();
    }
    else
    {
        supports_multiple_images_ = supports_animation_ = false;
    }
}

void Png::handle_extra_args(const Args & args)
{
    auto options = Sub_args{"APNG"};
    try
    {
        options.add_options()
            ("not-composed", "Show only information for the given frame, not those leading up to it");

        auto sub_args = options.parse(args.extra_args);

        composed_ = !sub_args.count("not-composed");

        if(args.animate && !composed_)
            throw std::runtime_error{options.help(args.help_text) + "\nCan't specify --not-composed with --animate"};
    }
    catch(const cxxopt_exception & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}

void write_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        png_longjmp(png_ptr, 1);

    out->write(reinterpret_cast<char *>(data), length);
    if(out->bad())
        png_longjmp(png_ptr, 1);
}
void flush_fn(png_structp png_ptr)
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        png_longjmp(png_ptr, 1);

    out->flush();
}
void Png::write(std::ostream & out, const Image & img, bool invert)
{
    const Image * img_p = &img;

    std::unique_ptr<Image> img_copy{nullptr};
    if(invert)
    {
        img_copy = std::make_unique<Image>(img.get_width(), img.get_height());
        for(std::size_t row = 0; row < img_copy->get_height(); ++row)
        {
            for(std::size_t col = 0; col < img_copy->get_width(); ++col)
            {
                FColor fcolor {img[row][col]};
                if(invert)
                    fcolor.invert();
                (*img_copy)[row][col] = fcolor;
            }
        }
        img_p = img_copy.get();
    }

    std::vector<const unsigned char *> row_ptrs(img_p->get_height());
    for(std::size_t i = 0; i < img_p->get_height(); ++i)
        row_ptrs[i] = reinterpret_cast<decltype(row_ptrs)::value_type>(std::data((*img_p)[i]));

    auto libpng = Libpng{Libpng::Type::WRITE};
    if(setjmp(png_jmpbuf(libpng)))
    {
        throw std::runtime_error{"Error writing with libpng"};
    }

    // set custom write callbacks to write to std::ostream
    png_set_write_fn(libpng, &out, write_fn, flush_fn);

    png_set_IHDR(libpng, libpng,
                 img.get_width(), img.get_height(),
                 8, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_rows(libpng, libpng, const_cast<png_bytepp>(std::data(row_ptrs)));

    png_write_png(libpng, libpng, PNG_TRANSFORM_IDENTITY, nullptr);

    png_write_end(libpng, nullptr); // TODO: multiple IENDs?
}
