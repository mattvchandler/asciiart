#ifndef PKMN_GEN1_HPP
#define PKMN_GEN1_HPP

#include <map>

#include "image.hpp"

class Pkmn_gen1 final: public Image
{
public:
    Pkmn_gen1() = default;
    void open(std::istream & input, const Args & args) override;

    void handle_extra_args(const Args & args) override;

    static void write(std::ostream & out, const Image & img, bool invert);

    static const std::map<std::string, std::array<Color, 4>> palettes;

private:
    unsigned int override_tile_width_ {0}, override_tile_height_ {0};
    bool check_overrun_ {true};
    bool fixed_buffer_ {false};

    inline static std::array<Color, 4> palette_entries_;
};
#endif // PKMN_GEN1_HPP
