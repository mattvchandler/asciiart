#ifndef COLOR_HPP
#define COLOR_HPP

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

#include <cmath>

// from boost::hash_combine
inline std::size_t hash_combine(std::size_t a, std::size_t b)
{
    if constexpr(sizeof(std::size_t) == 8)
        a^= b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
    else
        a^= b + 0x9e3779b9ul + (a << 6) + (a >> 2);
    return a;
}

struct Color
{
    unsigned char r{0}, g{0}, b{0}, a{0xFF};
    constexpr Color(){}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xFF): r{r}, g{g}, b{b}, a{a} {}
    constexpr explicit Color(unsigned char y): r{y}, g{y}, b{y} {}

    constexpr const unsigned char & operator[](unsigned char i) const
    {
        switch(i)
        {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        case 3:
            return a;
        default:
            throw std::logic_error{"color index out of bounds"};
        }
    }
    constexpr unsigned char & operator[](unsigned char i)
    {
        switch(i)
        {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        case 3:
            return a;
        default:
            throw std::logic_error{"color index out of bounds"};
        }
    }

    Color & invert()
    {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;

        return *this;
    }

    constexpr bool operator<(const Color & other) const
    {
        if(r != other.r)
            return r < other.r;
        else if(g != other.g)
            return g < other.g;
        else if(b != other.b)
            return b < other.b;
        else
            return a < other.a;
    }

    constexpr bool operator==(const Color & other) const
    {
        return r == other.r
            && g == other.g
            && b == other.b
            && a == other.a;
    }

    Color & operator+=(const Color & other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
        return *this;
    }
    Color & operator-=(const Color & other)
    {
        r -= other.r;
        g -= other.g;
        b -= other.b;
        a -= other.a;
        return *this;
    }

    Color & operator*=(decltype(r) other)
    {
        r *= other;
        g *= other;
        b *= other;
        a *= other;
        return *this;
    }
    Color & operator/=(decltype(r) other)
    {
        r /= other;
        g /= other;
        b /= other;
        a /= other;
        return *this;
    }
    Color & operator%=(decltype(r) other)
    {
        r %= other;
        g %= other;
        b %= other;
        a %= other;
        return *this;
    }

    Color operator+(const Color & other) const
    {
        return Color{*this} += other;
    }
    Color operator-(const Color & other) const
    {
        return Color{*this} -= other;
    }

    Color operator*(decltype(r) other) const
    {
        return Color{*this} *= other;
    }
    Color operator/(decltype(r) other) const
    {
        return Color{*this} /= other;
    }
    Color operator%(decltype(r) other) const
    {
        return Color{*this} %= other;
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
        if(r != other.r)
            return r < other.r;
        else if(g != other.g)
            return g < other.g;
        else if(b != other.b)
            return b < other.b;
        else
            return a < other.a;
    }

    FColor & alpha_blend(float bg)
    {
        r = r * a + bg * (1.0f - a);
        g = g * a + bg * (1.0f - a);
        b = b * a + bg * (1.0f - a);
        a = 1.0f;
        return *this;
    }
    float to_gray() const
    {
        // formulas from https://www.w3.org/TR/WCAG20/
        std::array<float, 3> luminance_color = { r, g, b };

        for(auto && c: luminance_color)
            c = (c <= 0.03928f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);

        return 0.2126f * luminance_color[0] + 0.7152f * luminance_color[1] + 0.0722f * luminance_color[2];
    }
    FColor & invert()
    {
        r = 1.0f - r;
        g = 1.0f - g;
        b = 1.0f - b;

        return *this;
    }

    FColor & operator+=(const FColor & other)
    {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
        return *this;
    }
    FColor & operator-=(const FColor & other)
    {
        r -= other.r;
        g -= other.g;
        b -= other.b;
        a -= other.a;
        return *this;
    }

    FColor & operator*=(float other)
    {
        r *= other;
        g *= other;
        b *= other;
        a *= other;
        return *this;
    }
    FColor & operator/=(float other)
    {
        r /= other;
        g /= other;
        b /= other;
        a /= other;
        return *this;
    }

    FColor operator+(const FColor & other) const
    {
        return FColor{*this} += other;
    }
    FColor operator-(const FColor & other) const
    {
        return FColor{*this} -= other;
    }

    FColor operator*(decltype(r) other) const
    {
        return FColor{*this} *= other;
    }
    FColor operator/(decltype(r) other) const
    {
        return FColor{*this} /= other;
    }

    FColor & clamp()
    {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);
        return *this;
    }
};

namespace std
{
    template<> struct hash<Color>
    {
        auto operator()(const Color & c) const
        {
            return hash_combine(hash_combine(hash_combine(hash<decltype(c.r)>{}(c.r), hash<decltype(c.g)>{}(c.g)), hash<decltype(c.b)>{}(c.b)), hash<decltype(c.a)>{}(c.a));
        }
    };
    template<> struct hash<FColor>
    {
        auto operator()(const FColor & c) const
        {
            return hash_combine(hash_combine(hash_combine(hash<decltype(c.r)>{}(c.r), hash<decltype(c.g)>{}(c.g)), hash<decltype(c.b)>{}(c.b)), hash<decltype(c.a)>{}(c.a));
        }
    };
}

inline float color_dist2(const FColor & a, const FColor & b)
{
    return (a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b) + (a.a - b.a) * (a.a - b.a);
}

inline float color_dist(const FColor & a, const FColor & b)
{
    return std::sqrt(color_dist2(a, b));
}

#endif // COLOR_HPP
