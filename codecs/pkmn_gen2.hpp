#ifndef PKMN_GEN2_HPP
#define PKMN_GEN2_HPP

#include "image.hpp"

class Pkmn_gen2 final: public Image
{
public:
    Pkmn_gen2() = default;
    void open(std::istream & input, const Args & args) override;

    void handle_extra_args(const Args & args) override;

private:
    unsigned int tile_width_ {0}, tile_height_ {0};

    bool palette_set_ {false};
    std::array<Color, 4> palette_entries_;
};
#endif // PKMN_GEN2_HPP
