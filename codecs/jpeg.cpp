#include "jpeg.hpp"

#include <algorithm>
#include <iostream>
#include <span>
#include <stdexcept>

#include <csetjmp>

#include <jpeglib.h>

#include "binio.hpp"

#ifdef EXIF_FOUND
#include "exif.hpp"
#endif

struct my_jpeg_error: public jpeg_error_mgr
{
    jmp_buf setjmp_buffer;
    static void exit(j_common_ptr cinfo) noexcept
    {
        cinfo->err->output_message(cinfo);
        std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);
    }
};

struct Libjpeg
{
    my_jpeg_error jerr;

    inline static std::string error_msg = {"Generic error"};

    Libjpeg()
    {
        jerr.error_exit = my_jpeg_error::exit;
    }
    virtual ~Libjpeg() = default;
    Libjpeg(const Libjpeg &) = delete;
    Libjpeg & operator=(const Libjpeg &) = delete;
    Libjpeg(Libjpeg && other): jerr{std::move(other.jerr)} {}
    Libjpeg & operator=(Libjpeg && other)
    {
        if(this != &other)
            jerr = std::move(other.jerr);
        return *this;
    }

    void set_error_point(const std::string & msg)
    {
        error_msg = msg;
        if(setjmp(jerr.setjmp_buffer))
            throw std::runtime_error{error_msg};
    }
};

class Libjpeg_read: public Libjpeg
{
private:
    bool initialized_ {false};
    jpeg_decompress_struct cinfo_;
public:
    Libjpeg_read():
        initialized_{true}
    {
        jpeg_create_decompress(&cinfo_);
        cinfo_.err = jpeg_std_error(&jerr);
    }
    ~Libjpeg_read()
    {
        if(initialized_)
            jpeg_destroy_decompress(&cinfo_);
    }
    Libjpeg_read(const Libjpeg_read &) = delete;
    Libjpeg_read & operator=(const Libjpeg_read &) = delete;
    Libjpeg_read(Libjpeg_read && other):
        initialized_{true},
        cinfo_{std::move(other.cinfo_)}
    {
        other.initialized_ = false;
    }
    Libjpeg_read & operator=(Libjpeg_read && other)
    {
        if(this != &other)
        {
            std::swap(cinfo_, other.cinfo_);
        }
        return *this;
    }

    operator jpeg_decompress_struct *() { return &cinfo_; }
    operator jpeg_decompress_struct const *() const { return &cinfo_; }

    jpeg_decompress_struct * operator->() { return &cinfo_; }
    const jpeg_decompress_struct * operator->() const { return &cinfo_; }
};

class my_jpeg_source: public jpeg_source_mgr
{
public:
    explicit my_jpeg_source(std::istream & input):
        input_{input}
    {
        init_source = [](j_decompress_ptr){};
        fill_input_buffer = my_fill_input_buffer;
        skip_input_data = my_skip_input_data;
        resync_to_restart = jpeg_resync_to_restart;
        term_source = [](j_decompress_ptr){};
        bytes_in_buffer = 0;
        next_input_byte = nullptr;
    }

private:
    static boolean my_fill_input_buffer(j_decompress_ptr cinfo) noexcept
    {
        auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

        src.input_.read(reinterpret_cast<char *>(std::data(src.buffer_)), std::size(src.buffer_));

        src.next_input_byte = std::data(src.buffer_);
        src.bytes_in_buffer = src.input_.gcount();

        if(src.input_.bad() || src.bytes_in_buffer == 0)
        {
            std::cerr<<"ERROR: Could not read JPEG image\n";
            src.buffer_[0] = 0xFF;
            src.buffer_[1] = JPEG_EOI;
            src.bytes_in_buffer = 2;
            return false;
        }

        return true;
    }

    static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes) noexcept
    {
        if(num_bytes > 0)
        {
            auto &src = *static_cast<my_jpeg_source*>(cinfo->src);

            while(src.bytes_in_buffer < static_cast<decltype(src.bytes_in_buffer)>(num_bytes))
            {
                num_bytes -= src.bytes_in_buffer;
                my_fill_input_buffer(cinfo);
            }

            src.next_input_byte += num_bytes;
            src.bytes_in_buffer -= num_bytes;
        }
    }

    std::istream & input_;
    std::array<JOCTET, 4096> buffer_;
};

struct MPF
{
    std::uint32_t num_images {0};
    std::uint32_t individual_image_no = {1};
    struct Entry
    {
        bool parent {false};
        bool child {false};
        bool representative{false};
        enum class Type: std::uint32_t { Baseline_primary = 0x030000,
                                         Large_thumb_VGA = 0x010001, Large_thumb_HD = 0x010002,
                                         Multi_frame_pano = 0x02001, Multi_frame_disparity = 0x020002, Multi_frame_multi_angle = 0x020003,
                                         Undefined = 0x000000};
        Type type {Type::Undefined};

        std::uint32_t size {0}, offset{0};
    };
    std::vector<Entry> entries;
};

std::optional<MPF> parse_mpf(const std::span<JOCTET> & mpf)
{
    // MPF format specification at https://www.cipa.jp/std/documents/e/DC-X007-KEY_E.pdf
    // TIFF-style tag specification at https://www.fileformat.info/format/tiff/egff.htm

    using namespace std::string_view_literals;
    auto data = std::begin(mpf);

    try
    {
        if(readstr(data, std::end(mpf), 4) != "MPF\0"sv) // MPF magic number
            return {}; // not an MPF

        auto offset_ref = data;

        auto endian = std::endian::little;

        auto mp_endian = readstr(data, std::end(mpf), 4);
        if(mp_endian == "II*\x0"sv)
            endian = std::endian::little;
        else if(mp_endian == "MM\0*"sv)
            endian = std::endian::big;
        else
            return {}; // unknown endian tag;

        auto ifd_offset = readb<std::uint32_t>(data, std::end(mpf), endian);
        data = offset_ref + ifd_offset;
        if(data >= std::end(mpf))
            return {};

        MPF out;

        while(true)
        {
            auto ifd_count = readb<std::uint16_t>(data, std::end(mpf), endian);
            for(decltype(ifd_count) ifd_i = 0u; ifd_i < ifd_count; ++ifd_i)
            {
                auto id = readb<std::uint16_t>(data, std::end(mpf), endian);
                data += sizeof(std::uint16_t); // skip type, not useful in practice
                if(data >= std::end(mpf))
                    return {};
                auto count = readb<std::uint32_t>(data, std::end(mpf), endian);

                switch(id)
                {
                    case 0xB000: // MP format version #
                        if(readstr(data, std::end(mpf), 4) != "0100"sv)
                            return {}; // illegal version #
                        break;
                    case 0xB001: // number of images
                        out.num_images = readb<std::uint32_t>(data, std::end(mpf), endian);
                        out.entries.resize(out.num_images);
                        break;
                    case 0xB002: // MP entries
                        {
                            if(count / 16 != out.num_images)
                                return {};
                            auto entry_offset = readb<std::uint32_t>(data, std::end(mpf), endian);
                            auto entry = offset_ref + entry_offset;
                            if(entry >= std::end(mpf))
                                return {};
                            for(decltype(out.num_images) entry_i = 0u; entry_i < out.num_images; ++entry_i)
                            {
                                auto & e = out.entries[entry_i];
                                auto attr = readb<std::uint32_t>(entry, std::end(mpf), endian);
                                e.parent         = attr & 0x8000000000u;
                                e.child          = attr & 0x4000000000u;
                                e.representative = attr & 0x2000000000u;
                                auto type = attr & 0x00FFFFFFu;
                                switch(type)
                                {
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Baseline_primary):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Large_thumb_VGA):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Large_thumb_HD):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Multi_frame_pano):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Multi_frame_disparity):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Multi_frame_multi_angle):
                                    case static_cast<std::underlying_type_t<MPF::Entry::Type>>(MPF::Entry::Type::Undefined):
                                        e.type = static_cast<MPF::Entry::Type>(type);
                                        break;
                                    default:
                                        return {}; // Illegal type
                                }

                                e.size = readb<std::uint32_t>(entry, std::end(mpf), endian);
                                e.offset = readb<std::uint32_t>(entry, std::end(mpf), endian);
                                entry += 2 * sizeof(std::uint16_t); // skip dependent_img<n>_entry_number fields
                                if(entry >= std::end(mpf))
                                    return {};
                            }
                        }
                        break;
                    case 0xB101: // Individual image number
                        out.individual_image_no = readb<std::uint32_t>(data, std::end(mpf), endian);
                        break;
                    case 0xB003: // Image UID list
                    case 0xB004: // total frames
                    case 0xB201: // panorama scanning orientation
                    case 0xB202: // panorama horiz overlap
                    case 0xB203: // panorama vert overlap
                    case 0xB204: // base viewpoint #
                    case 0xB205: // convergence angle
                    case 0xB206: // baseline length
                    case 0xB207: // divergence angle
                    case 0xB208: // horiz axis distance
                    case 0xB209: // vert axis distance
                    case 0xB20A: // collimation axis distance
                    case 0xB20B: // yaw angle
                    case 0xB20C: // pitch angle
                    case 0xB20D: // roll angle
                        // ignore
                        data += sizeof(std::uint32_t);
                        if(data >= std::end(mpf))
                            return {};
                        break;
                    default: // illegal
                        return {};
                }
            }

            ifd_offset = readb<std::uint32_t>(data, std::end(mpf), endian);
            if(ifd_offset == 0)
                break;
            data = offset_ref + ifd_offset;
            if(data >= std::end(mpf))
                return {};
        }
        return out;
    }
    catch(const std::runtime_error & e)
    {
        if(e.what() == "Unexpected end of input"sv)
            return {};
        else
            throw;
    }
}

void Jpeg::open(std::istream & input, const Args & args)
{
    my_jpeg_source source(input);

    std::optional<MPF> parent_mpf;
    std::vector<std::pair<Jpeg, MPF>> images;
    auto image_no = 0u;

    do
    {
        auto cinfo = Libjpeg_read{};
        cinfo.set_error_point("Error reading header with libjpeg");

        cinfo->src = &source;

        // request APP1 for EXIF for rotation, and APP2 for MPF extra image data
    #ifdef EXIF_FOUND
        jpeg_save_markers(cinfo, JPEG_APP0 + 1, 0xFFFF);
    #endif
        jpeg_save_markers(cinfo, JPEG_APP0 + 2, 0xFFFF);

        jpeg_read_header(cinfo, true);
        cinfo->out_color_space = JCS_RGB;

        auto orientation {exif::Orientation::r_0};
        std::optional<MPF> mpf;
        for(auto marker = cinfo->marker_list; marker; marker = marker->next)
        {
            switch(marker->marker)
            {
        #ifdef EXIF_FOUND
            // get orientation from EXIF, if it exists
            case JPEG_APP0 + 1:
                orientation = exif::get_orientation(marker->data, marker->data_length).value_or(orientation);
                break;
        #endif
            case JPEG_APP0 + 2:
                    mpf = parse_mpf(std::span{marker->data, marker->data_length});
                break;
            default:
                break;
            }
        }

        if(mpf)
        {
            if(!parent_mpf && mpf->num_images > 0)
                parent_mpf = mpf;
        }
        else if(parent_mpf)
            throw std::runtime_error{"Error reading MPF JPEG: Secondary image missing MNF info"};

        cinfo.set_error_point("Error reading JPEG");

        jpeg_start_decompress(cinfo);

        set_size(cinfo->output_width, cinfo->output_height);

        if(cinfo->output_components != 3)
            throw std::runtime_error{"JPEG not converted to RGB"};

        std::vector<unsigned char> buffer(cinfo->output_width * 3);

        while(cinfo->output_scanline < cinfo->output_height)
        {
            auto ptr = std::data(buffer);
            jpeg_read_scanlines(cinfo, &ptr, 1);
            for(std::size_t i = 0; i < cinfo->output_width; ++i)
                image_data_[cinfo->output_scanline - 1][i] = Color{buffer[i * 3], buffer[i * 3 + 1], buffer[i * 3 + 2]};
        }

        // rotate as needed
        transpose_image(orientation);
        jpeg_finish_decompress(cinfo);

        if(parent_mpf && mpf && parent_mpf->num_images > 0)
        {
            images.emplace_back(*this, *mpf);
            // seek ahead for next SOI (0xFFD8)
            if(image_no + 1 < parent_mpf->num_images)
            {
                while(true)
                {
                    if(source.bytes_in_buffer == 0)
                    {
                        if(!source.fill_input_buffer(cinfo))
                            throw std::runtime_error{"Error reading MPF JPEG: Unexpected end of file while seeking to next image"};
                    }
                    if(*source.next_input_byte == 0xFF)
                        break;

                    source.skip_input_data(cinfo, 1);
                }
            }
        }

    } while(parent_mpf && ++image_no < parent_mpf->num_images);

    if(!std::empty(images))
    {
        auto req_image_no = args.image_no.value_or(0u);
        if(req_image_no >= std::size(images))
            throw std::runtime_error{"Error reading MNF JPEG: image " + std::to_string(req_image_no) + " is out of range (0-" + std::to_string(std::size(images)) + ")"};

        std::sort(std::begin(images), std::end(images), [](auto && a, auto && b) { return a.second.individual_image_no < b.second.individual_image_no; });
        for(auto i = 0u; i < std::size(images); ++i)
        {
            if(images[i].second.individual_image_no != i + 1)
                throw std::runtime_error{"Error reading MNF JPEG: non-sequential image numbers"};
        }

        *this = images[req_image_no].first;
    }
    else if(args.image_no)
    {
        throw std::runtime_error{args.help_text + "\nImage type doesn't support multiple images"};
    }
}

class Libjpeg_write: public Libjpeg
{
private:
    bool initialized_ {false};
    jpeg_compress_struct cinfo_;
public:
    Libjpeg_write():
        initialized_{true}
    {
        jpeg_create_compress(&cinfo_);
        cinfo_.err = jpeg_std_error(&jerr);
    }
    ~Libjpeg_write()
    {
        if(initialized_)
            jpeg_destroy_compress(&cinfo_);
    }
    Libjpeg_write(const Libjpeg_write &) = delete;
    Libjpeg_write & operator=(const Libjpeg_write &) = delete;
    Libjpeg_write(Libjpeg_write && other):
        initialized_{true},
        cinfo_{std::move(other.cinfo_)}
    {
        other.initialized_ = false;
    }
    Libjpeg_write & operator=(Libjpeg_write && other)
    {
        if(this != &other)
        {
            std::swap(cinfo_, other.cinfo_);
        }
        return *this;
    }

    operator jpeg_compress_struct *() { return &cinfo_; }
    operator jpeg_compress_struct const *() const { return &cinfo_; }

    jpeg_compress_struct * operator->() { return &cinfo_; }
    const jpeg_compress_struct * operator->() const { return &cinfo_; }
};

class my_jpeg_dest: public jpeg_destination_mgr
{
public:
    explicit my_jpeg_dest(std::ostream & output):
        output_{output}
    {
        init_destination = init;
        empty_output_buffer = empty_buffer;
        term_destination = term;
        next_output_byte = nullptr;
        free_in_buffer = 0;
    }

private:

    static void init(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);
        dest.next_output_byte = std::data(dest.buffer_);
        dest.free_in_buffer = std::size(dest.buffer_);
    }

    static boolean empty_buffer(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);
        dest.output_.write(reinterpret_cast<char *>(std::data(dest.buffer_)), std::size(dest.buffer_));
        if(dest.output_.bad())
            std::longjmp(static_cast<my_jpeg_error *>(cinfo->err)->setjmp_buffer, 1);

        dest.next_output_byte = std::data(dest.buffer_);
        dest.free_in_buffer = std::size(dest.buffer_);

        return true;
    }

    static void term(j_compress_ptr cinfo)
    {
        auto & dest = *static_cast<my_jpeg_dest*>(cinfo->dest);

        if(dest.free_in_buffer < std::size(dest.buffer_))
        {
            dest.output_.write(reinterpret_cast<char *>(std::data(dest.buffer_)), std::size(dest.buffer_) - dest.free_in_buffer);
            dest.output_.flush();
        }
    }

    std::ostream & output_;
    std::array<JOCTET, 4096> buffer_;
};

void Jpeg::write(std::ostream & out, const Image & img, unsigned char bg, bool invert)
{
    std::vector<unsigned char> buffer(img.get_width() * 3);

    my_jpeg_dest dest(out);

    auto cinfo = Libjpeg_write{};
    cinfo.set_error_point("Error writing with libjpeg");

    cinfo->dest = &dest;
    cinfo->image_width = img.get_width();
    cinfo->image_height = img.get_height();
    cinfo->input_components = 3;
    cinfo->in_color_space = JCS_RGB;

    jpeg_set_defaults(cinfo);

    jpeg_start_compress(cinfo, true);

    while(cinfo->next_scanline < cinfo->image_height)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            FColor fcolor = img[cinfo->next_scanline][col];
            if(invert)
                fcolor.invert();

            fcolor.alpha_blend(bg / 255.0f);

            buffer[col * 3    ] = static_cast<unsigned char>(fcolor.r * 255.0f);
            buffer[col * 3 + 1] = static_cast<unsigned char>(fcolor.g * 255.0f);
            buffer[col * 3 + 2] = static_cast<unsigned char>(fcolor.b * 255.0f);
        }

        auto buffer_ptr = std::data(buffer);
        jpeg_write_scanlines(cinfo, &buffer_ptr, 1);
    }

    jpeg_finish_compress(cinfo);
}
