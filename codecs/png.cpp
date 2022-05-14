#include "png.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <csetjmp>
#include <cstdint>

#include <png.h>

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

#include "../animate.hpp"
#include "sub_args.hpp"

/*
void read_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto in = static_cast<std::istream *>(png_get_io_ptr(png_ptr));
    if(!in)
        std::longjmp(png_jmpbuf(png_ptr), 1);

    in->read(reinterpret_cast<char *>(data), length);
    if(in->bad())
        std::longjmp(png_jmpbuf(png_ptr), 1);
}
void Png::open(std::istream & input, const Args &)
{
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }
    auto end_info_ptr = png_create_info_struct(png_ptr);
    if(!end_info_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_strucpng_get_eXIf_1(png_ptr, info_ptr, &exif_length, &exif) != 0 ||t(&png_ptr, &info_ptr, &end_info_ptr);
        throw std::runtime_error{"Error reading with libpng"};
    }

    // set custom read callback (to read from header / c++ istream)
    png_set_read_fn(png_ptr, &input, read_fn);

    // get image properties
    png_read_info(png_ptr, info_ptr);

    set_size(png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr));

    auto bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    auto color_type = png_get_color_type(png_ptr, info_ptr);

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

    auto number_of_passes = png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    if(png_get_rowbytes(png_ptr, info_ptr) != width_ * 4)
        throw std::runtime_error{"PNG bytes per row incorrect"};

    for(decltype(number_of_passes) pass = 0; pass < number_of_passes; ++pass)
    {
        for(size_t row = 0; row < height_; ++row)
        {
            png_read_row(png_ptr, reinterpret_cast<unsigned char*>(std::data(image_data_[row])), NULL);
        }
    }

    #ifdef EXIF_FOUND
    auto orientation { exif::Orientation::r_0};

    png_bytep exif = nullptr;
    png_uint_32 exif_length = 0;

    png_read_end(png_ptr, end_info_ptr);
    if(png_get_eXIf_1(png_ptr, info_ptr, &exif_length, &exif) != 0 || png_get_eXIf_1(png_ptr, end_info_ptr, &exif_length, &exif) != 0)
    {
        if (exif_length > 1)
        {
            std::vector<unsigned char> exif_buf(exif_length + 6);
            for(auto i = 0; i < 6; ++i)
                exif_buf[i] = "Exif\0\0"[i];
            std::copy(exif, exif + exif_length, std::begin(exif_buf) + 6);

            orientation = exif::get_orientation(std::data(exif_buf), std::size(exif_buf));
        }
    }

    transpose_image(orientation);
    #endif

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
}
*/

enum class Dispose_op: std::uint8_t {NONE = 0u, BACKGROUND, PREVIOUS};
enum class Blend_op: std::uint8_t {SOURCE = 0u, OVER};

struct Animation_info
{
    Png * img;
    const Args & args;

    bool is_apng {false};
    unsigned int num_frames {0};
    unsigned int num_plays  {0};

    std::vector<png_byte> copied_chunks;
#ifdef EXIF_FOUND
    exif::Orientation orientation { exif::Orientation::r_0};
#endif

    // current frame data
    bool fctl_set {false};
    bool idat_set {false};
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

    Animation_info(Png * img, const Args & args):
        img{img},
        args{args}
    {}

    // void start_frame()
    // {
    //     auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    //     if(!png_ptr)
    //     {
    //         std::cerr<<"Error initializing libpng for APNG frame decoding"<<'\n';
    //         std::longjmp(png_jmpbuf(png_ptr), 1);
    //     }

    //     auto info_ptr = png_create_info_struct(png_ptr);
    //     if(!info_ptr)
    //     {
    //         png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    //         std::cerr<<"Error initializing libpng info for APNG frame decoding"<<'\n';
    //         std::longjmp(png_jmpbuf(png_ptr), 1);
    //     }

    //     if(setjmp(png_jmpbuf(png_ptr)))
    //     {
    //         std::cerr<<"Error reading APNG frame with libpng"<<'\n';
    //         std::longjmp(png_jmpbuf(png_ptr), 1);
    //     }

    //     png_set_progressive_read_fn(png_ptr, this, info_callback, row_callback, nullptr);

    //     png_process_data(png_ptr, info_ptr, std::data(copied_chunks), std::size(copied_chunks));
    // }
    // void end_frame()
    // {
    //     if(png_ptr)
    //     {
    //         png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    //         png_ptr  = nullptr;
    //         info_ptr = nullptr;
    //     }
    // }
};

void Png::open(std::istream & input, const Args & args)
{
    auto info_callback = [](png_structp png_ptr, png_infop info_ptr)
    {
        auto animation_info = reinterpret_cast<Animation_info *>(png_get_progressive_ptr(png_ptr));

        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type, compression_type, filter_type;
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_type);

        // if(animation_info->img->width_ == 0 && animation_info->img->height_ == 0)
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

        if(!animation_info->idat_set)
        {
            animation_info->idat_set = true;
            animation_info->include_default_image = animation_info->fctl_set;
        }

        png_progressive_combine_row(png_ptr, reinterpret_cast<png_bytep>(std::data(animation_info->img->image_data_[row_num])), new_row);
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
            animation_info->fctl_set = true;

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

    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        throw std::runtime_error{"Error reading with libpng"};
    }

    png_set_progressive_read_fn(png_ptr, &animation_info, info_callback, row_callback, nullptr);
    png_set_read_user_chunk_fn(png_ptr, &animation_info, chunk_callback);
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, std::data(self_handled_chunks), std::size(self_handled_chunks) / 5);

    while(input)
    {
        input.read(std::data(io_buffer), std::size(io_buffer));
        if(input.bad())
        {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            throw std::runtime_error {"Error reading PNG file"};
        }

        png_process_data(png_ptr, info_ptr, reinterpret_cast<png_bytep>(std::data(io_buffer)), input.gcount());
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    png_ptr = nullptr;
    info_ptr = nullptr;

    if(!animation_info.is_apng && args.image_no)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support multiple images"};
    if(!animation_info.is_apng && args.animate)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support animation"};

    if(animation_info.is_apng && (args.animate || args.image_no))
    {
        std::sort(std::begin(animation_info.frame_chunks), std::end(animation_info.frame_chunks), [](auto && a, auto && b){ return a.seq_no < b.seq_no; });
        Animation_info::Frame_chunk frame_ctrl;
        struct Frame
        {
            Png img;
            unsigned int x_offset{0};
            unsigned int y_offset{0};
            float delay {0.0f};
            Dispose_op dispose_op {Dispose_op::NONE};
            Blend_op blend_op {Blend_op::SOURCE};

            Frame() = default;
            void set(const Animation_info::Frame_chunk & fc)
            {
                x_offset = fc.x_offset;
                y_offset = fc.y_offset;

                if(fc.delay_den == 0)
                    delay = static_cast<float>(fc.delay_num) / 100.0f;
                else
                    delay = static_cast<float>(fc.delay_num) / static_cast<float>(fc.delay_den);
            }
        };
        auto frames = std::vector<Frame>(animation_info.num_frames);
        auto frame_no = 0u;

        for(auto i = 0u; i < std::size(animation_info.frame_chunks); ++i)
        {
            auto & fc = animation_info.frame_chunks[i];

            // scan for gaps/non seq
            if(fc.seq_no != i)
            {
                if(png_ptr)
                    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

                throw std::runtime_error {"Error reading APNG file: missing chunks"};
            }

            if(std::empty(fc.fdat))
            {
                if(i == 0 && animation_info.include_default_image)
                {
                    frames[frame_no].img = *this;
                    frames[frame_no++].set(fc);
                }
                else
                {
                    if(png_ptr)
                    {
                        constexpr auto iend = std::array<png_byte, 12> {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0};
                        png_process_data(png_ptr, info_ptr, const_cast<png_bytep>(std::data(iend)), std::size(iend));
                        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
                        png_ptr = nullptr;
                        info_ptr = nullptr;
                    }
                    frame_ctrl = fc;

                    if(fc.width == 0 || fc.height == 0 ||
                            fc.x_offset + fc.width > width_ ||
                            fc.y_offset + fc.height > height_)
                    {
                        throw std::runtime_error{"Error reading APNG: Invalid frame dimensions\n"};
                    }

                    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
                    if(!png_ptr)
                        throw std::runtime_error{"Error initializing libpng"};

                    info_ptr = png_create_info_struct(png_ptr);
                    if(!info_ptr)
                    {
                        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
                        throw std::runtime_error{"Error initializing libpng info"};
                    }

                    // TODO: fix or at least silence clobbering warning for frame_no and i
                    if(setjmp(png_jmpbuf(png_ptr)))
                    {
                        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
                        throw std::runtime_error{"Error decoding APNG frames"};
                    }

                    animation_info.img = &frames[frame_no].img;
                    frames[frame_no++].set(fc);

                    png_set_progressive_read_fn(png_ptr, &animation_info, info_callback, row_callback, nullptr);
                    png_set_read_user_chunk_fn(png_ptr, &animation_info, chunk_callback);
                    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
                    png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE); // we're going to be feeding this garbage CRC values, so tell libpng to ignore them

                    png_save_int_32(std::data(animation_info.copied_chunks) + 16, fc.width);
                    png_save_int_32(std::data(animation_info.copied_chunks) + 20, fc.height);
                    png_process_data(png_ptr, info_ptr, std::data(animation_info.copied_chunks), std::size(animation_info.copied_chunks));
                }
            }
            else
            {
                auto scratch_buffer = std::array<png_byte, 4>{};
                auto idat_tag = std::array<png_byte, 4>{'I', 'D', 'A', 'T'};

                png_save_int_32(std::data(scratch_buffer), std::size(fc.fdat));

                png_process_data(png_ptr, info_ptr, std::data(scratch_buffer), std::size(scratch_buffer));
                png_process_data(png_ptr, info_ptr, std::data(idat_tag), std::size(idat_tag));
                png_process_data(png_ptr, info_ptr, std::data(fc.fdat), std::size(fc.fdat));
                png_process_data(png_ptr, info_ptr, std::data(scratch_buffer), std::size(scratch_buffer)); // garbage CRC
            }
        }
        if(png_ptr)
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

        auto image_no = args.image_no.value_or(0u);
        if(image_no >= animation_info.num_frames)
            throw std::runtime_error{"Error reading GIF: frame " + std::to_string(image_no) + " is out of range (0-" + std::to_string(animation_info.num_frames - 1) + ")"};

        bool do_loop = args.animate && args.loop_animation;
        auto animator = std::unique_ptr<Animate>{};
        auto start_frame = composed_ ? 0u : image_no;
        auto frame_end = image_no + 1;
        if(args.animate)
        {
            frame_end = animation_info.num_frames;
            animator = std::make_unique<Animate>(args);
        }

        // set to transparent black
        for(std::size_t row = 0; row < height_; ++row)
        {
            for(std::size_t col = 0; col < width_; ++col)
                image_data_[row][col] = Color{0u, 0u, 0u, 0u};
        }

        auto play = 0u;
        do
        {
            for(auto f = start_frame; f < frame_end; ++f)
            {
                for(std::size_t row = 0; row < frames[f].img.height_; ++row)
                {
                    for(std::size_t col = 0; col < frames[f].img.width_; ++col)
                    {
                        // TODO: blending / discard ops
                        image_data_[row + frames[f].y_offset][col + frames[f].x_offset] = frames[f].img.image_data_[row][col];
                    }
                }
                if(args.animate)
                {
                    animator->set_frame_delay(frames[f].delay);
                    animator->display(*this);
                    if(!animator->running())
                        break;
                }
            }
        } while(do_loop && animator && animator->running() && (++play < animation_info.num_plays || animation_info.num_plays == 0));
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

        if(args.animate && args.image_no)
            throw std::runtime_error{options.help(args.help_text) + "\nCan't specify --image-no with --animate"};
    }
    catch(const cxxopts::OptionException & e)
    {
        throw std::runtime_error{options.help(args.help_text) + '\n' + e.what()};
    }
}

void write_fn(png_structp png_ptr, png_bytep data, png_size_t length) noexcept
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        std::longjmp(png_jmpbuf(png_ptr), 1);

    out->write(reinterpret_cast<char *>(data), length);
    if(out->bad())
        std::longjmp(png_jmpbuf(png_ptr), 1);
}
void flush_fn(png_structp png_ptr)
{
    auto out = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    if(!out)
        std::longjmp(png_jmpbuf(png_ptr), 1);

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

    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr)
        throw std::runtime_error{"Error initializing libpng"};

    auto info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, nullptr);
        throw std::runtime_error{"Error initializing libpng info"};
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        throw std::runtime_error{"Error writing with libpng"};
    }

    // set custom write callbacks to write to std::ostream
    png_set_write_fn(png_ptr, &out, write_fn, flush_fn);

    png_set_IHDR(png_ptr, info_ptr,
                 img.get_width(), img.get_height(),
                 8, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_rows(png_ptr, info_ptr, const_cast<png_bytepp>(std::data(row_ptrs)));

    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

    png_write_end(png_ptr, nullptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
}
