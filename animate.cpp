#include "animate.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "display.hpp"

#ifdef HAS_SELECT
#include <sys/select.h>
#endif
#ifdef HAS_SIGNAL
#include <signal.h>
#endif
#ifdef HAS_TERMIOS
#include <termios.h>
#endif
#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#define ESC "\x1B"
#define CSI ESC "["
#define ENABLED "h"
#define DISABLED "l"
#define SGR "m"
#define ALT_BUFF CSI "?1049"
#define CURSOR CSI "?25"
#define CLS CSI "2J"
#define SEP ";"
#define CUP "H"
#define RESET_CHAR CSI "0" SGR
#define DISABLE_ECHO CSI "8" SGR

class Animate::Animate_impl
{
public:
    explicit Animate_impl(const Args & args);
    ~Animate_impl();
    Animate_impl(const Animate_impl &) = delete;
    Animate_impl & operator=(const Animate_impl &) = delete;
    Animate_impl(Animate_impl &&) = delete;
    Animate_impl & operator=(Animate_impl &&) = delete;

    void display(const Image & img);
    void set_frame_delay(float delay_s);

    bool running() const;

private:
    Args args_;
    std::chrono::duration<float, std::ratio<1,1>> frame_delay_{1.0f / 30.0f};
    std::chrono::high_resolution_clock::time_point last_frame_time_{std::chrono::high_resolution_clock::time_point::min()};
#ifdef HAS_TERMIOS
    termios old_term_info_;
#endif
    bool running_ {true};

    void open_alternate_buffer();
    void close_alternate_buffer();
    void set_signals();
    void reset_signals();
    void reset_cursor_pos() const;
};

namespace
{
    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
    volatile sig_atomic_t stop_flag = 0;
    volatile sig_atomic_t suspend_flag = 0;

    void handle_stop(int)    { stop_flag    = 1; }
    void handle_suspend(int) { suspend_flag = 1; }
    #endif

    void set_signal(int sig, void(*handler)(int))
    {
    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
        std::string sigstr;
        #define CASESTR(x) case x: sigstr = #x; break;
        switch(sig)
        {
            CASESTR(SIGINT)
            CASESTR(SIGTERM)
            CASESTR(SIGTSTP)
            default: sigstr = std::to_string(sig); break;
        }
        #undef CASESTR

        struct sigaction action{};

        if(sigaction(sig, nullptr, &action) == -1)
            throw std::runtime_error{std::string{"Could not get signal "} + sigstr  + ": " + std::strerror(errno)};

        if(!(action.sa_flags & SA_SIGINFO) && action.sa_handler == SIG_IGN)
            throw std::runtime_error{std::string{"Signal "} + sigstr  + " is ignored"};

        if(!(action.sa_flags & SA_SIGINFO) && action.sa_handler != SIG_DFL)
            throw std::runtime_error{std::string{"Signal "} + sigstr  + " is already handled"};

        sigemptyset(&action.sa_mask);
        action.sa_flags &= ~SA_SIGINFO;
        action.sa_handler = handler;

        if(sigaction(sig, &action, nullptr) == -1)
            throw std::runtime_error{std::string{"Could not set signal "} + sigstr + ": " + std::strerror(errno)};
    #endif
    }

    void reset_signal(int sig)
    {
    #if defined(HAS_SELECT) && defined(HAS_SIGNAL)
        signal(sig, SIG_DFL);
    #endif
    }
}

Animate::Animate(const Args & args):
    pimpl{std::make_unique<Animate::Animate_impl>(args)}
{}
Animate::Animate_impl::Animate_impl(const Args & args):
    args_{args}
{
#ifdef HAS_UNISTD
    if(!isatty(fileno(stdout)))
        throw std::runtime_error{"Can't animate - not a TTY"};
#endif

    set_signals();
    open_alternate_buffer();
}

Animate::~Animate() = default; // needed for unique_ptr as pimpl
Animate::Animate_impl::~Animate_impl()
{
    close_alternate_buffer();
    reset_signals();
}

void Animate::display(const Image & img) { pimpl->display(img); }
void Animate::Animate_impl::display(const Image & img)
{
    reset_cursor_pos();
    print_image(img, args_, std::cout);
    std::cout.flush();

#if defined(HAS_SELECT) && defined(HAS_SIGNAL)
    if(suspend_flag)
    {
        close_alternate_buffer();
        reset_signal(SIGTSTP);
        raise(SIGTSTP);

        set_signal(SIGTSTP,  handle_suspend);
        open_alternate_buffer();
        suspend_flag = 0;
        last_frame_time_ = decltype(last_frame_time_)::min();
    }

    if(stop_flag)
    {
        running_ = false;
        return;
    }
#endif

    auto frame_end = std::chrono::high_resolution_clock::now();
    auto frame_time = std::max(decltype(frame_delay_)::zero(), std::chrono::duration_cast<decltype(frame_delay_)>(frame_end-last_frame_time_));
    auto sleep_time = frame_delay_ - frame_time;

    std::this_thread::sleep_for(sleep_time);
    last_frame_time_ = std::chrono::high_resolution_clock::now();
}

void Animate::set_framerate(float fps) { pimpl->set_frame_delay(1.0f / fps); }
void Animate::set_frame_delay(float delay_s) { pimpl->set_frame_delay(delay_s); }
void Animate::Animate_impl::set_frame_delay(float delay_s) { frame_delay_ = decltype(frame_delay_){delay_s}; }

bool Animate::running() const { return pimpl->running(); }
bool Animate::Animate_impl::running() const { return running_; }

void Animate::Animate_impl::open_alternate_buffer()
{
    std::cout <<ALT_BUFF ENABLED CLS CURSOR DISABLED DISABLE_ECHO << std::flush;

#ifdef HAS_TERMIOS
    tcgetattr(STDIN_FILENO, &old_term_info_); // save old term attrs
    setvbuf(stdin, nullptr, _IONBF, 0);
    auto newt = old_term_info_;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
}

void Animate::Animate_impl::close_alternate_buffer()
{
    std::cout<<ALT_BUFF ENABLED<<std::flush;
    std::cout<<CLS ALT_BUFF DISABLED CURSOR ENABLED RESET_CHAR<<std::flush;

#ifdef HAS_TERMIOS
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term_info_);
#endif
}
void Animate::Animate_impl::set_signals()
{
#if defined(HAS_SELECT) && defined(HAS_SIGNAL)
    set_signal(SIGINT,   handle_stop);
    set_signal(SIGTERM,  handle_stop);
    set_signal(SIGTSTP,  handle_suspend);
#endif
}
void Animate::Animate_impl::reset_signals()
{
#if defined(HAS_SELECT) && defined(HAS_SIGNAL)
    reset_signal(SIGINT);
    reset_signal(SIGTERM);
    reset_signal(SIGTSTP);
#endif
}

void Animate::Animate_impl::reset_cursor_pos() const { std::cout << CSI CUP; }
