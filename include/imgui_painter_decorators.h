/* Stock Dear ImGui widget decoration. The callable submits the real widget;
 * imgui-painter suppresses only its stock chrome and paints replacement
 * chrome underneath its text without disturbing the last-item contract. */
#ifndef IMGUI_PAINTER_DECORATORS_H
#define IMGUI_PAINTER_DECORATORS_H

#include "imgui.h"
#include "imgui_painter.h"
#include "imgui_painter_recipes.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>

/* Unlike imgui_painter_imgui.h's stable host-value APIs, these decorators
 * reconstruct widget chrome geometry that Dear ImGui does not expose as an
 * upstream contract. The Rust reference's debug-only check disappears in
 * release builds; a compile-time failure with a named, visual-gate opt-out
 * prevents a version mismatch from silently producing wrong geometry. */
#ifndef IMGUI_PAINTER_ALLOW_UNVERIFIED_IMGUI
static_assert(IMGUI_VERSION_NUM == 19191,
              "imgui-painter decorator chrome geometry is verified only against "
              "Dear ImGui 1.91.9b; define IMGUI_PAINTER_ALLOW_UNVERIFIED_IMGUI "
              "to build anyway after rerunning the visual gate");
#endif

namespace ip {
namespace detail {

enum class Decorator { Button, Selectable };

struct ItemState {
    ImVec2 min;
    ImVec2 max;
    bool hovered;
    bool active;
    bool focused;
    float style_alpha;
};

/* Carries its own length: the suppression sets are not all the same size
 * (Slider suppresses five roles, Combo six), and a guard that pushed one
 * count while popping another would silently unbalance ImGui's style stack. */
struct SuppressedColors {
    const ImGuiCol *data;
    int count;
};

inline SuppressedColors suppressed_colors(Decorator decorator) {
    static constexpr ImGuiCol button_colors[] = {
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive};
    static constexpr ImGuiCol header_colors[] = {
        ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive};
    if (decorator == Decorator::Button) {
        return {button_colors, static_cast<int>(std::size(button_colors))};
    }
    return {header_colors, static_cast<int>(std::size(header_colors))};
}

/* Kept as an explicit pre-submit operation even though Group 1 widgets use
 * their post-submit item rectangle. Later multipart decorators need this
 * exact call site to capture their chrome before submission. */
inline std::optional<ip_rect> capture_chrome(Decorator) { return std::nullopt; }

class StyleColorGuard {
public:
    explicit StyleColorGuard(Decorator decorator) {
        static constexpr ImVec4 transparent{0.0f, 0.0f, 0.0f, 0.0f};
        const SuppressedColors colors = suppressed_colors(decorator);
        count_ = colors.count;
        for (int index = 0; index < count_; ++index) {
            ImGui::PushStyleColor(colors.data[index], transparent);
        }
    }

    ~StyleColorGuard() { restore(); }

    StyleColorGuard(const StyleColorGuard &) = delete;
    StyleColorGuard &operator=(const StyleColorGuard &) = delete;

    void restore() {
        if (count_ > 0) {
            ImGui::PopStyleColor(count_);
            count_ = 0;
        }
    }

private:
    int count_ = 0;
};

class ChannelSplitGuard {
public:
    explicit ChannelSplitGuard(ImDrawList *draw_list)
        : draw_list_(draw_list), active_(true) {
        draw_list_->ChannelsSplit(3);
        draw_list_->ChannelsSetCurrent(1);
    }

    ~ChannelSplitGuard() { merge(); }

    ChannelSplitGuard(const ChannelSplitGuard &) = delete;
    ChannelSplitGuard &operator=(const ChannelSplitGuard &) = delete;

    void background() { draw_list_->ChannelsSetCurrent(0); }

    void merge() {
        if (active_) {
            draw_list_->ChannelsMerge();
            active_ = false;
        }
    }

private:
    ImDrawList *draw_list_;
    bool active_;
};

inline ItemState capture_item_state() {
    return ItemState{ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                     ImGui::IsItemHovered(), ImGui::IsItemActive(),
                     ImGui::IsItemFocused(), ImGui::GetStyle().Alpha};
}

inline ip_rect item_rect(const ItemState &state) {
    return {{state.min.x, state.min.y}, {state.max.x, state.max.y}};
}

inline float rect_height(ip_rect value) { return value.max.y - value.min.y; }

inline bool rect_is_valid(ip_rect value) {
    return std::isfinite(value.min.x) && std::isfinite(value.min.y) &&
           std::isfinite(value.max.x) && std::isfinite(value.max.y) &&
           value.max.x >= value.min.x && value.max.y >= value.min.y;
}

inline bool rect_contains(ip_rect outer, ip_rect inner) {
    constexpr float epsilon = 0.5f;
    return inner.min.x >= outer.min.x - epsilon &&
           inner.min.y >= outer.min.y - epsilon &&
           inner.max.x <= outer.max.x + epsilon &&
           inner.max.y <= outer.max.y + epsilon;
}

struct SingleAnatomy {
    ip_rect chrome;
};

inline SingleAnatomy single_anatomy(const ItemState &state,
                                    const std::optional<ip_rect> &captured) {
    assert(!captured.has_value());
    (void)captured;
    const ip_rect chrome = item_rect(state);
    assert(rect_is_valid(chrome));
    assert(rect_contains(item_rect(state), chrome));
    return {chrome};
}

inline ip_color with_style_alpha(ip_color color, float style_alpha) {
    const float alpha = std::clamp(style_alpha, 0.0f, 1.0f);
    const auto source = static_cast<std::uint8_t>((color >> 24u) & 0xffu);
    const auto resolved = static_cast<std::uint8_t>(
        std::round(static_cast<float>(source) * alpha));
    return (color & 0x00ffffffu) | (static_cast<ip_color>(resolved) << 24u);
}

inline ip_border resolved_border(ip_border border, float alpha) {
    border.color = with_style_alpha(border.color, alpha);
    return border;
}

inline ip_shadow resolved_shadow(ip_shadow shadow, float alpha) {
    shadow.color = with_style_alpha(shadow.color, alpha);
    return shadow;
}

inline ip_color state_fill(const StateColors &colors, const ItemState &state) {
    return state.active ? colors.active : (state.hovered ? colors.hover : colors.base);
}

enum class SelectableVisualState { Pressed, Selected, Hovered, Idle };

inline SelectableVisualState selectable_visual_state(const ItemState &state,
                                                      bool selected) {
    if (state.active) {
        return SelectableVisualState::Pressed;
    }
    if (selected) {
        return SelectableVisualState::Selected;
    }
    if (state.hovered) {
        return SelectableVisualState::Hovered;
    }
    return SelectableVisualState::Idle;
}

inline ip_color selectable_fill(const Material &material, const ItemState &state,
                                bool selected) {
    switch (selectable_visual_state(state, selected)) {
    case SelectableVisualState::Pressed:
        return selected ? shade(material.fill.active, 0.12f) : material.fill.active;
    case SelectableVisualState::Selected:
        return material.fill.active;
    case SelectableVisualState::Hovered:
        return material.fill.hover;
    case SelectableVisualState::Idle:
        return material.fill.base;
    }
    return material.fill.base;
}

template <typename DrawList>
void paint_material_color(Canvas<DrawList> &canvas, ip_rect rect,
                          const Material &material, ip_color color,
                          float style_alpha) {
    assert(rect_is_valid(rect));
    canvas.rounded_rect(rect, std::min(material.radius, rect_height(rect) * 0.5f));
    if (material.shadow) {
        canvas.shadow(resolved_shadow(*material.shadow, style_alpha));
    }
    canvas.fill(with_style_alpha(color, style_alpha));
    canvas.border(resolved_border(material.border, style_alpha));
}

template <typename DrawList>
void paint_material(Canvas<DrawList> &canvas, ip_rect rect,
                    const Material &material, const ItemState &state) {
    paint_material_color(canvas, rect, material, state_fill(material.fill, state),
                         state.style_alpha);
}

template <typename Widget, typename Paint>
auto item_paint(Frame &frame, Decorator decorator, Widget &&widget, Paint &&paint)
    -> decltype(widget()) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const std::optional<ip_rect> captured = capture_chrome(decorator);
    ChannelSplitGuard channels(draw_list);
    StyleColorGuard colors(decorator);

    decltype(widget()) result = std::forward<Widget>(widget)();
    const ItemState state = capture_item_state();
    colors.restore();

    channels.background();
    {
        auto canvas = frame.canvas(*draw_list);
        std::forward<Paint>(paint)(state, captured, canvas);
    }
    channels.merge();
    return std::forward<decltype(result)>(result);
}

} // namespace detail

template <typename Widget>
auto decorate_button(Frame &frame, const Material &material, Widget &&widget)
    -> decltype(widget()) {
    return detail::item_paint(
        frame, detail::Decorator::Button, std::forward<Widget>(widget),
        [&material](const detail::ItemState &state,
                    const std::optional<ip_rect> &captured, auto &canvas) {
            const detail::SingleAnatomy anatomy =
                detail::single_anatomy(state, captured);
            detail::paint_material(canvas, anatomy.chrome, material, state);
        });
}

template <typename Widget>
auto decorate_selectable(Frame &frame, const Material &material, bool selected,
                         Widget &&widget) -> decltype(widget()) {
    return detail::item_paint(
        frame, detail::Decorator::Selectable, std::forward<Widget>(widget),
        [&material, selected](const detail::ItemState &state,
                              const std::optional<ip_rect> &captured, auto &canvas) {
            const detail::SingleAnatomy anatomy =
                detail::single_anatomy(state, captured);
            detail::paint_material_color(
                canvas, anatomy.chrome, material,
                detail::selectable_fill(material, state, selected), state.style_alpha);
        });
}

} // namespace ip

#endif /* IMGUI_PAINTER_DECORATORS_H */
