#include "mng.hpp"

#include <istream>
#include <limits>
#include <stdexcept>
#include <string>

#include <cstdlib>

#include <libmng.h>

#include "../animate.hpp"

class Libmng
{
private:
    mng_handle mng_ {nullptr};

    static mng_ptr mng_alloc(mng_size_t len) noexcept { return std::calloc(1u, len); }
    static void mng_free(mng_ptr p, mng_size_t) noexcept { std::free(p); }

public:

    Libmng(void * user)
    {
        mng_ = mng_initialize(user, mng_alloc, mng_free, nullptr);
        if(!mng_)
        {
            throw std::runtime_error{"Error initializing libmng"};
        }
    }
    ~Libmng()
    {
        if(mng_)
            mng_cleanup(&mng_);
    }

    Libmng(const Libmng &) = delete;
    Libmng & operator=(const Libmng &) = delete;

    Libmng(Libmng && other):
        mng_{std::move(other.mng_)}
    {
        other.mng_ = nullptr;
    }

    Libmng & operator=(Libmng && other)
    {
        if(this != &other)
        {
            mng_ = std::move(other.mng_);
            other.mng_ = nullptr;
        }
        return *this;
    }

    [[noreturn]] void throw_error(const std::string & where = ": ")
    {
        mng_int8 severity;
        mng_chunkid chunk_name;
        mng_uint32 chunk_seq;
        mng_int32 extra1, extra2;
        mng_pchar text;

        mng_getlasterror(mng_, &severity, &chunk_name, &chunk_seq, &extra1, &extra2, &text);
        throw std::runtime_error{"libmng error" + where + (text ? text : "Unspecified error")};
    }

    operator mng_handle() { return mng_; }
    operator mng_data_struct const *() const { return mng_; }
};

void Mng::open(std::istream & input, const Args & args)
{
    struct Mng_info
    {
        Mng & mng;
        std::istream & input;
        mng_uint32 faked_timer {0};

        bool hit_mend {false};
        std::vector<Mng> frames;

        Mng_info(Mng & mng, std::istream & input):
            mng{mng},
            input{input}
        {}
    };
    auto mng_info = Mng_info{*this, input};

    auto mng = std::make_unique<Libmng>(&mng_info);

    if(mng_set_canvasstyle(*mng, MNG_CANVAS_RGBA8) != MNG_NOERROR)
        mng->throw_error(" on setting canvas: ");

    if(mng_setcb_openstream(*mng, [](mng_handle) noexcept -> mng_bool { return MNG_TRUE; }) != MNG_NOERROR)
        mng->throw_error(" on setting stream_cb: ");
    if(mng_setcb_closestream(*mng, [](mng_handle) noexcept -> mng_bool { return MNG_TRUE; }) != MNG_NOERROR)
        mng->throw_error(" on setting stream_cb: ");
    if(mng_setcb_readdata(*mng, [](mng_handle handle, mng_ptr buf, mng_uint32 size, mng_uint32p bytes_read) noexcept -> mng_bool
    {
        auto mng_info = reinterpret_cast<Mng_info*>(mng_get_userdata(handle));
        mng_info->input.read(reinterpret_cast<char *>(buf), size);
        *bytes_read = mng_info->input.gcount();
        return MNG_TRUE;
    }) != MNG_NOERROR)
        mng->throw_error(" on setting read_cb: ");

    if(mng_setcb_processheader(*mng, [](mng_handle handle, mng_uint32 width, mng_uint32 height) -> mng_bool
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        mng_info->mng.set_size(width, height);

        return MNG_TRUE;
    }) != MNG_NOERROR)
        mng->throw_error(" setting header callback: ");

    if(mng_setcb_gettickcount(*mng, [](mng_handle handle) -> mng_uint32
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        return mng_info->faked_timer;
    }) != MNG_NOERROR)
        mng->throw_error(" setting gettickcount callback: ");

    if(mng_setcb_settimer(*mng, [](mng_handle handle, mng_uint32 duration) -> mng_bool
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        mng_info->faked_timer += duration;

        return MNG_TRUE;
    })  != MNG_NOERROR)
        mng->throw_error(" setting setttimer callback: ");

    if(mng_setcb_getcanvasline(*mng, [](mng_handle handle, mng_uint32 row) -> mng_ptr
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        return reinterpret_cast<mng_ptr>(std::data(mng_info->mng.image_data_[row]));
    }) != MNG_NOERROR)
        mng->throw_error(" setting row callback: ");

    if(mng_setcb_refresh(*mng, [](mng_handle handle, mng_uint32, mng_uint32, mng_uint32, mng_uint32) -> mng_bool
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        mng_info->frames.emplace_back(mng_info->mng);

        return MNG_TRUE;
    }) != MNG_NOERROR)
        mng->throw_error(" setting refresh_callback ");

    if(mng_read(*mng) != MNG_NOERROR)
        mng->throw_error(" reading input: ");

    if(mng_setcb_processmend(*mng, [](mng_handle handle, mng_uint32, mng_uint32) -> mng_bool
    {
        auto mng_info = reinterpret_cast<Mng_info *>(mng_get_userdata(handle));

        mng_info->hit_mend = true;

        return MNG_TRUE;
    }) != MNG_NOERROR)
        mng->throw_error(" setting mend callback ");

    auto disp_status = mng_display(*mng);
    while(disp_status == MNG_NEEDTIMERWAIT)
    {
        disp_status = mng_display_resume(*mng);
        if(mng_info.hit_mend)
        {
            disp_status = MNG_NOERROR;
            break;
        }
    }
    if(disp_status != MNG_NOERROR)
        mng->throw_error(" on retrieving frames: ");

    auto frame_delay = mng_get_ticks(*mng) ? mng_get_ticks(*mng) / 1000.0f : 1.0f / 30.0f;
    mng.reset();

    auto & frames = mng_info.frames;
    if(std::size(frames) <= 1u && args.image_no)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support multiple images"};
    if(std::size(frames) <= 1u && args.animate)
        throw std::runtime_error{args.help_text + "\nImage type doesn't support animation"};

    auto image_no = args.image_no.value_or(0u);

    if(image_no >= std::size(frames))
        throw std::runtime_error{"Error reading MNG: frame " + std::to_string(image_no) + " is out of range (0-" + std::to_string(std::size(frames) - 1) + ")"};

    if(args.animate)
    {
        auto animator = Animate{args};
        do
        {
            for(auto && f: frames)
            {
                animator.set_frame_delay(args.animation_frame_delay > 0.0f ? args.animation_frame_delay : frame_delay);
                animator.display(f);
                if(!animator)
                    break;
            }
        } while(animator && args.loop_animation);
    }
    else
    {
        *this = std::move(frames[image_no]);
    }
}
