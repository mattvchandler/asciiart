#ifndef ANIMATE_HPP
#define ANIMATE_HPP

#include <memory>

#include "args.hpp"
#include "codecs/image.hpp"

class Animate
{
public:
    explicit Animate(const Args & args);

    ~Animate();
    Animate(const Animate &) = delete;
    Animate & operator=(const Animate &) = delete;
    Animate(Animate &&) = delete;
    Animate & operator=(Animate &&) = delete;

    void display(const Image & img);
    void set_framerate(float fps);
    void set_frame_delay(float delay_s);

    bool running() const;
    operator bool() const { return running(); }

private:
    class Animate_impl;
    std::unique_ptr<Animate_impl> pimpl;
};
#endif // ANIMATE_HPP
