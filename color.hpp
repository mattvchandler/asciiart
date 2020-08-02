#ifndef COLOR_HPP
#define COLOR_HPP

#include <array>
#include <map>

#include <cmath>

struct Color
{
    unsigned char r{0}, g{0}, b{0}, a{0xFF};
    constexpr Color(){}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xFF): r{r}, g{g}, b{b}, a{a} {}
    constexpr explicit Color(unsigned char y): r{y}, g{y}, b{y} {}

    constexpr bool operator<(const Color & other) const
    {
        if(r < other.r)
            return true;
        else if(g < other.g)
            return true;
        else if(b < other.b)
            return true;
        else if(a < other.a)
            return true;
        else
            return false;
    }
};

struct FColor
{
    float r{0.0f}, g{0.0f}, b{0.0f}, a{1.0f};
    constexpr FColor(){}
    constexpr FColor(float r, float g, float b, float a = 1.0f):
        r{r},
        g{g},
        b{b},
        a{a}
    {}
    constexpr FColor(const Color & c):
        r{c.r / 255.0f},
        g{c.g / 255.0f},
        b{c.b / 255.0f},
        a{c.a / 255.0f}
    {}

    constexpr operator Color() const
    {
        return {
            static_cast<unsigned char>(r * 255.0f),
            static_cast<unsigned char>(g * 255.0f),
            static_cast<unsigned char>(b * 255.0f),
            static_cast<unsigned char>(a * 255.0f)
        };
    }

    constexpr bool operator<(const FColor & other) const
    {
        if(r < other.r)
            return true;
        else if(g < other.g)
            return true;
        else if(b < other.b)
            return true;
        else if(a < other.a)
            return true;
        else
            return false;
    }

    void alpha_blend(float bg)
    {
        r = r * a + bg * (1.0f - a);
        g = g * a + bg * (1.0f - a);
        b = b * a + bg * (1.0f - a);
        a = 1.0f;
    }
    float to_gray() const
    {
        // formulas from https://www.w3.org/TR/WCAG20/
        std::array<float, 3> luminance_color = { r, g, b };

        for(auto && c: luminance_color)
            c = (c <= 0.03928f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);

        return 0.2126f * luminance_color[0] + 0.7152f * luminance_color[1] + 0.0722f * luminance_color[2];
    }
    void invert()
    {
        r = 1.0f - r;
        g = 1.0f - g;
        b = 1.0f - b;
    }
};

inline float color_dist(const FColor & a, const FColor & b)
{
    auto key = std::pair{a, b};
    static std::map<decltype(key), float> lru_cache;

    if(auto lru = lru_cache.find(key); lru != std::end(lru_cache))
        return lru->second;

    auto dist = std::sqrt((a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b));
    lru_cache.emplace(key, dist);

    return dist;
}

#endif // COLOR_HPP
