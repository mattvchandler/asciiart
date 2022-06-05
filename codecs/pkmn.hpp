#ifndef PKMN_HPP
#define PKMN_HPP

#include "image.hpp"

class Pkmn final: public Image
{
public:
    Pkmn() = default;
    void open(std::istream & input, const Args & args) override;

    void handle_extra_args(const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);

private:
    unsigned int override_tile_width_ {0}, override_tile_height_ {0};
    bool check_overrun_ {true};
    bool fixed_buffer_ {false};

    inline static std::string palette_ {"grey"}; // TODO: need handle_extra_args for output
};
#endif // PKMN_HPP
