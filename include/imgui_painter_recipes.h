/* Small chrome recipes derived from host-owned palette tokens. */
#ifndef IMGUI_PAINTER_RECIPES_H
#define IMGUI_PAINTER_RECIPES_H

#include "imgui_painter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace ip {

/* A minimal chrome token palette. imgui-painter paints chrome only; text
 * and text_muted exist so hosts can keep typography coherent through their
 * stock text-style APIs. imgui-painter itself never paints text. */
struct Palette {
    ip_color surface;
    ip_color surface_raised;
    ip_color surface_inset;
    ip_color border_light;
    ip_color border_dark;
    ip_color accent;
    ip_color selection;
    ip_color text;
    ip_color text_muted;
};

struct StateColors {
    ip_color base, hover, active;
};

struct Material {
    float radius;
    StateColors fill;
    ip_border border;
    std::optional<ip_shadow> shadow;
};

struct SliderStyle {
    Material track, fill, grab;
};

struct ComboStyle {
    Material frame, arrow_region;
};

struct TreeStyle {
    Material row, disclosure;
};

namespace detail {

inline std::uint8_t channel(ip_color color, std::uint32_t shift) {
    return static_cast<std::uint8_t>((color >> shift) & 0xffu);
}

inline ip_color shade(ip_color color, float amount) {
    const float scale = 1.0f - amount;
    const auto r = static_cast<std::uint8_t>(
        std::round(static_cast<float>(channel(color, 0u)) * scale));
    const auto g = static_cast<std::uint8_t>(
        std::round(static_cast<float>(channel(color, 8u)) * scale));
    const auto b = static_cast<std::uint8_t>(
        std::round(static_cast<float>(channel(color, 16u)) * scale));
    return static_cast<ip_color>(r) | (static_cast<ip_color>(g) << 8u) |
           (static_cast<ip_color>(b) << 16u) |
           (static_cast<ip_color>(channel(color, 24u)) << 24u);
}

inline ip_color tint(ip_color color, float amount) {
    const auto lift = [amount](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::round(
            static_cast<float>(value) +
            static_cast<float>(std::uint8_t{255} - value) * amount));
    };
    const auto r = lift(channel(color, 0u));
    const auto g = lift(channel(color, 8u));
    const auto b = lift(channel(color, 16u));
    return static_cast<ip_color>(r) | (static_cast<ip_color>(g) << 8u) |
           (static_cast<ip_color>(b) << 16u) |
           (static_cast<ip_color>(channel(color, 24u)) << 24u);
}

inline ip_color mix(ip_color a, ip_color b, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto blend = [a, b, amount](std::uint32_t shift) {
        const float a_channel = static_cast<float>(channel(a, shift));
        const float b_channel = static_cast<float>(channel(b, shift));
        return static_cast<std::uint8_t>(
            std::round(a_channel + (b_channel - a_channel) * amount));
    };
    const auto r = blend(0u);
    const auto g = blend(8u);
    const auto b_channel = blend(16u);
    const auto alpha = blend(24u);
    return static_cast<ip_color>(r) | (static_cast<ip_color>(g) << 8u) |
           (static_cast<ip_color>(b_channel) << 16u) |
           (static_cast<ip_color>(alpha) << 24u);
}

inline ip_color with_alpha(ip_color color, std::uint8_t alpha) {
    return (color & 0x00ffffffu) | (static_cast<ip_color>(alpha) << 24u);
}

} // namespace detail

inline Material raised_button(const Palette &palette) {
    return Material{
        3.0f,
        {palette.surface_raised, detail::tint(palette.surface_raised, 0.10f),
         detail::shade(palette.surface_raised, 0.14f)},
        {1.0f, palette.border_dark},
        ip_shadow{{0.0f, 1.0f}, 3.0f, 0.0f, detail::with_alpha(palette.border_dark, 96u),
                  false},
    };
}

inline Material toolbar_button(const Palette &palette) {
    return Material{
        2.0f,
        {palette.surface, palette.surface_raised, palette.surface_inset},
        {1.0f, palette.border_dark},
        std::nullopt,
    };
}

inline Material inset_control(const Palette &palette) {
    return Material{
        2.0f,
        {palette.surface_inset, detail::tint(palette.surface_inset, 0.06f),
         detail::shade(palette.surface_inset, 0.08f)},
        {1.0f, palette.border_dark},
        ip_shadow{{0.0f, 1.0f}, 3.0f, 0.0f, detail::with_alpha(palette.border_dark, 112u),
                  true},
    };
}

inline Material selected_row(const Palette &palette) {
    return Material{
        1.0f,
        {palette.surface, detail::tint(palette.selection, 0.08f), palette.selection},
        {1.0f, palette.border_dark},
        std::nullopt,
    };
}

inline TreeStyle browser_tree_row(const Palette &palette) {
    return TreeStyle{
        selected_row(palette),
        Material{
            1.0f,
            {palette.surface_inset, detail::tint(palette.selection, 0.08f),
             palette.selection},
            {1.0f, palette.border_dark},
            std::nullopt,
        },
    };
}

inline SliderStyle parameter_slider(const Palette &palette) {
    return SliderStyle{
        inset_control(palette),
        Material{
            2.0f,
            {palette.accent, detail::tint(palette.accent, 0.10f),
             detail::shade(palette.accent, 0.12f)},
            {1.0f, detail::shade(palette.accent, 0.28f)},
            std::nullopt,
        },
        raised_button(palette),
    };
}

inline ComboStyle combo_field(const Palette &palette) {
    return ComboStyle{inset_control(palette), raised_button(palette)};
}

template <typename DrawList>
void panel(Canvas<DrawList> &canvas, ip_rect rect, const Palette &palette) {
    const float hairline = canvas.device_pixel();
    canvas.rounded_rect(rect, 5.0f);
    canvas.shadow(ip_shadow{{0.0f, 3.0f}, 9.0f, 0.0f,
                            detail::with_alpha(palette.border_dark, 104u), false});
    const ip_color_stop stops[2] = {
        {0.0f, palette.surface_raised},
        {1.0f, palette.surface},
    };
    const ip_gradient gradient{
        IP_GRADIENT_LINEAR, {rect.min.x, rect.min.y}, {rect.min.x, rect.max.y}, stops, 2,
    };
    canvas.fill(gradient);
    canvas.band(rect.min.y + hairline, rect.min.y + hairline * 2.0f, palette.border_light);
    canvas.border(ip_border{hairline, palette.border_dark});
}

template <typename DrawList>
void inset_panel(Canvas<DrawList> &canvas, ip_rect rect, const Palette &palette) {
    const float hairline = canvas.device_pixel();
    canvas.rounded_rect(rect, 3.0f);
    canvas.fill(palette.surface_inset);
    canvas.shadow(ip_shadow{{0.0f, 2.0f}, 5.0f, hairline,
                            detail::with_alpha(palette.border_dark, 128u), true});
    canvas.border(ip_border{hairline, palette.border_dark});
    canvas.band(rect.max.y - hairline * 2.0f, rect.max.y - hairline,
                palette.border_light);
}

} // namespace ip

#endif /* IMGUI_PAINTER_RECIPES_H */
