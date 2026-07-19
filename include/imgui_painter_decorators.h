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

enum class Decorator {
    Button,
    Selectable,
    Checkbox,
    InputText,
    Slider,
    Combo,
    Tree
};

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
    static constexpr ImGuiCol frame_colors[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive};
    static constexpr ImGuiCol slider_colors[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
    static constexpr ImGuiCol combo_colors[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive};
    switch (decorator) {
    case Decorator::Button:
        return {button_colors, static_cast<int>(std::size(button_colors))};
    case Decorator::Selectable:
    case Decorator::Tree:
        return {header_colors, static_cast<int>(std::size(header_colors))};
    case Decorator::Checkbox:
    case Decorator::InputText:
        return {frame_colors, static_cast<int>(std::size(frame_colors))};
    case Decorator::Slider:
        return {slider_colors, static_cast<int>(std::size(slider_colors))};
    case Decorator::Combo:
        return {combo_colors, static_cast<int>(std::size(combo_colors))};
    }
    return {nullptr, 0};
}

/* These formulas reproduce Dear ImGui 1.91.9b layout through public
 * functions, but widget-part geometry is not an upstream contract. The
 * executable compatibility gate and visual gallery must be rerun on every
 * Dear ImGui bump. */
inline std::optional<ip_rect> capture_chrome(Decorator decorator) {
    float width = 0.0f;
    switch (decorator) {
    case Decorator::Button:
    case Decorator::Selectable:
    case Decorator::Tree:
        return std::nullopt;
    case Decorator::Checkbox:
        width = ImGui::GetFrameHeight();
        break;
    case Decorator::InputText:
    case Decorator::Slider:
    case Decorator::Combo:
        width = ImGui::CalcItemWidth();
        break;
    }
    const float height = ImGui::GetFrameHeight();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    return ip_rect{{min.x, min.y}, {min.x + width, min.y + height}};
}

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

inline float rect_width(ip_rect value) { return value.max.x - value.min.x; }

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

struct SliderAnatomy {
    ip_rect frame;
    ip_rect track;
    ip_rect fill;
    ip_rect grab;
};

struct ComboAnatomy {
    ip_rect frame;
    ip_rect preview;
    ip_rect arrow;
};

struct TreeAnatomy {
    ip_rect row;
    std::optional<ip_rect> disclosure;
};

inline ComboAnatomy combo_anatomy(ip_rect frame) {
    const float arrow_width =
        std::min(std::max(rect_height(frame), 0.0f),
                 std::max(rect_width(frame), 0.0f));
    const float split = frame.max.x - arrow_width;
    return {frame,
            {{frame.min.x, frame.min.y}, {split, frame.max.y}},
            {{split, frame.min.y}, {frame.max.x, frame.max.y}}};
}

inline TreeAnatomy tree_anatomy(ip_rect row, bool leaf,
                                float disclosure_width) {
    std::optional<ip_rect> disclosure;
    if (!leaf) {
        disclosure = ip_rect{
            {row.min.x, row.min.y},
            {std::min(row.min.x + std::max(disclosure_width, 0.0f), row.max.x),
             row.max.y}};
    }
    return {row, disclosure};
}

inline float normalized_linear(float value, float min, float max) {
    assert(std::isfinite(value) && std::isfinite(min) && std::isfinite(max));
    if (!std::isfinite(value) || !std::isfinite(min) || !std::isfinite(max) ||
        min == max) {
        return 0.0f;
    }
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}

inline SliderAnatomy slider_anatomy(ip_rect frame, float min, float max,
                                    float value, float grab_min_size,
                                    float framebuffer_scale) {
    constexpr float grab_padding = 2.0f;
    const float frame_width = std::max(rect_width(frame), 0.0f);
    const float frame_height = std::max(rect_height(frame), 0.0f);
    const float slider_size = std::max(frame_width - grab_padding * 2.0f, 0.0f);
    const float grab_size =
        std::min(std::max(grab_min_size, 0.0f), slider_size);
    const float usable_size = std::max(slider_size - grab_size, 0.0f);
    const float usable_min = frame.min.x + grab_padding + grab_size * 0.5f;
    const float grab_center =
        usable_min + usable_size * normalized_linear(value, min, max);
    const ip_rect grab{
        {grab_center - grab_size * 0.5f,
         std::min(frame.min.y + grab_padding, frame.max.y)},
        {grab_center + grab_size * 0.5f,
         std::max(frame.max.y - grab_padding, frame.min.y)}};

    const float device_pixel =
        std::isfinite(framebuffer_scale) && framebuffer_scale > 0.0f
            ? 1.0f / framebuffer_scale
            : 1.0f;
    const float track_height = std::min(
        std::max(frame_height * 0.25f, device_pixel * 2.0f), frame_height);
    const float track_y = (frame.min.y + frame.max.y) * 0.5f;
    const ip_rect track{
        {std::min(frame.min.x + grab_padding, frame.max.x),
         track_y - track_height * 0.5f},
        {std::max(frame.max.x - grab_padding, frame.min.x),
         track_y + track_height * 0.5f}};
    const ip_rect fill{
        {track.min.x, track.min.y},
        {std::clamp(grab_center, track.min.x, track.max.x), track.max.y}};

    return {frame, track, fill, grab};
}

inline SingleAnatomy single_anatomy(Decorator decorator, const ItemState &state,
                                    const std::optional<ip_rect> &captured) {
    const ip_rect chrome =
        decorator == Decorator::Button || decorator == Decorator::Selectable
            ? item_rect(state)
            : captured.value();
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

enum class StateColorSlot { Base, Hover, Active };

inline ip_color state_color(const StateColors &colors, StateColorSlot slot) {
    switch (slot) {
    case StateColorSlot::Base:
        return colors.base;
    case StateColorSlot::Hover:
        return colors.hover;
    case StateColorSlot::Active:
        return colors.active;
    }
    return colors.base;
}

enum class SliderVisualState { Adjusting, Focused, Hovered, Idle };

enum class ComboVisualState { Open, Pressed, Focused, Hovered, Idle };

inline ComboVisualState combo_visual_state(const ItemState &state,
                                           bool popup_open) {
    if (popup_open) {
        return ComboVisualState::Open;
    }
    if (state.active) {
        return ComboVisualState::Pressed;
    }
    if (state.focused) {
        return ComboVisualState::Focused;
    }
    if (state.hovered) {
        return ComboVisualState::Hovered;
    }
    return ComboVisualState::Idle;
}

inline StateColorSlot combo_state_color_slot(ComboVisualState state) {
    switch (state) {
    case ComboVisualState::Idle:
        return StateColorSlot::Base;
    case ComboVisualState::Hovered:
    case ComboVisualState::Focused:
        return StateColorSlot::Hover;
    case ComboVisualState::Open:
    case ComboVisualState::Pressed:
        return StateColorSlot::Active;
    }
    return StateColorSlot::Base;
}

enum class TreeVisualState { Pressed, Selected, Focused, Hovered, Open, Idle };

/* Open ranks below Hovered/Focused: openness is already communicated by the
 * disclosure arrow, and letting it outrank hover made expanded rows feel
 * inert. It stays a named state (currently painting like Idle) so a future
 * distinct open treatment needs no re-plumbing. */
inline TreeVisualState tree_visual_state(const ItemState &state, bool selected,
                                         bool open) {
    if (state.active) {
        return TreeVisualState::Pressed;
    }
    if (selected) {
        return TreeVisualState::Selected;
    }
    if (state.focused) {
        return TreeVisualState::Focused;
    }
    if (state.hovered) {
        return TreeVisualState::Hovered;
    }
    if (open) {
        return TreeVisualState::Open;
    }
    return TreeVisualState::Idle;
}

inline StateColorSlot tree_state_color_slot(TreeVisualState state) {
    switch (state) {
    case TreeVisualState::Idle:
    case TreeVisualState::Open:
        return StateColorSlot::Base;
    case TreeVisualState::Hovered:
    case TreeVisualState::Focused:
        return StateColorSlot::Hover;
    case TreeVisualState::Pressed:
    case TreeVisualState::Selected:
        return StateColorSlot::Active;
    }
    return StateColorSlot::Base;
}

inline SliderVisualState slider_visual_state(const ItemState &state) {
    if (state.active) {
        return SliderVisualState::Adjusting;
    }
    if (state.focused) {
        return SliderVisualState::Focused;
    }
    if (state.hovered) {
        return SliderVisualState::Hovered;
    }
    return SliderVisualState::Idle;
}

inline StateColorSlot slider_state_color_slot(SliderVisualState state) {
    switch (state) {
    case SliderVisualState::Idle:
        return StateColorSlot::Base;
    case SliderVisualState::Hovered:
    case SliderVisualState::Focused:
        return StateColorSlot::Hover;
    case SliderVisualState::Adjusting:
        return StateColorSlot::Active;
    }
    return StateColorSlot::Base;
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

template <typename DrawList>
void paint_material_slot(Canvas<DrawList> &canvas, ip_rect rect,
                         const Material &material, StateColorSlot slot,
                         float style_alpha) {
    paint_material_color(canvas, rect, material,
                         state_color(material.fill, slot), style_alpha);
}

template <typename DrawList>
void paint_slider(Canvas<DrawList> &canvas, const SliderAnatomy &anatomy,
                  const SliderStyle &style, const ItemState &state,
                  SliderVisualState visual_state) {
    const StateColorSlot slot = slider_state_color_slot(visual_state);
    paint_material_slot(canvas, anatomy.track, style.track, slot,
                        state.style_alpha);
    if (rect_width(anatomy.fill) > 0.0f) {
        paint_material_slot(canvas, anatomy.fill, style.fill, slot,
                            state.style_alpha);
    }
    paint_material_slot(canvas, anatomy.grab, style.grab, slot,
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
                detail::single_anatomy(detail::Decorator::Button, state, captured);
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
                detail::single_anatomy(detail::Decorator::Selectable, state, captured);
            detail::paint_material_color(
                canvas, anatomy.chrome, material,
                detail::selectable_fill(material, state, selected), state.style_alpha);
        });
}

/* Decorates one Checkbox, painting only its box. The closure must submit
 * exactly one Checkbox. */
template <typename Widget>
auto decorate_checkbox(Frame &frame, const Material &material, Widget &&widget)
    -> decltype(widget()) {
    return detail::item_paint(
        frame, detail::Decorator::Checkbox, std::forward<Widget>(widget),
        [&material](const detail::ItemState &state,
                    const std::optional<ip_rect> &captured, auto &canvas) {
            const detail::SingleAnatomy anatomy = detail::single_anatomy(
                detail::Decorator::Checkbox, state, captured);
            detail::paint_material(canvas, anatomy.chrome, material, state);
        });
}

/* Decorates one single-line InputText, excluding its visible label. The
 * closure must submit exactly one single-line InputText. */
template <typename Widget>
auto decorate_input_text(Frame &frame, const Material &material, Widget &&widget)
    -> decltype(widget()) {
    return detail::item_paint(
        frame, detail::Decorator::InputText, std::forward<Widget>(widget),
        [&material](const detail::ItemState &state,
                    const std::optional<ip_rect> &captured, auto &canvas) {
            const detail::SingleAnatomy anatomy = detail::single_anatomy(
                detail::Decorator::InputText, state, captured);
            detail::paint_material(canvas, anatomy.chrome, material, state);
        });
}

/* Decorates one horizontal linear f32 Slider, painting its track, fill, and
 * grab. The closure must submit exactly one Slider using this value/range. */
template <typename Widget>
bool decorate_slider_f32(Frame &frame, const SliderStyle &style, float min,
                         float max, float &value, Widget &&widget) {
    return detail::item_paint(
        frame, detail::Decorator::Slider,
        [&value, &widget] { return std::forward<Widget>(widget)(value); },
        [&style, min, max, &value](const detail::ItemState &state,
                                  const std::optional<ip_rect> &captured,
                                  auto &canvas) {
            /* value() rather than assert + operator*: the reference fails
             * hard here in release too, and dereferencing an empty optional
             * would be undefined behaviour rather than a diagnosable one.
             * Matches single_anatomy's handling of the same invariant. */
            const ip_rect frame_rect = captured.value();
            const detail::SliderAnatomy anatomy = detail::slider_anatomy(
                frame_rect, min, max, value, ImGui::GetStyle().GrabMinSize,
                ImGui::GetIO().DisplayFramebufferScale.x);
            assert(detail::rect_contains(detail::item_rect(state), frame_rect));
            detail::paint_slider(canvas, anatomy, style, state,
                                 detail::slider_visual_state(state));
        });
}

/* Decorates one BeginCombo and, when open, runs its popup contents before
 * calling EndCombo. Suppressed parent colors are restored before contents
 * run, and the parent item state is captured only after EndCombo restores it.
 * Returns whether the popup was open. */
template <typename Begin, typename Contents>
bool decorate_combo(Frame &frame, const ComboStyle &style, Begin &&begin,
                    Contents &&contents) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ip_rect frame_rect =
        detail::capture_chrome(detail::Decorator::Combo).value();
    detail::ChannelSplitGuard channels(draw_list);
    detail::StyleColorGuard colors(detail::Decorator::Combo);

    const bool popup_open = std::forward<Begin>(begin)();
    colors.restore();
    if (popup_open) {
        try {
            std::forward<Contents>(contents)();
        } catch (...) {
            ImGui::EndCombo();
            throw;
        }
        ImGui::EndCombo();
    }

    const detail::ItemState state = detail::capture_item_state();
    assert(detail::rect_contains(detail::item_rect(state), frame_rect));
    const detail::ComboAnatomy anatomy = detail::combo_anatomy(frame_rect);
    const detail::StateColorSlot slot = detail::combo_state_color_slot(
        detail::combo_visual_state(state, popup_open));

    channels.background();
    {
        auto canvas = frame.canvas(*draw_list);
        detail::paint_material_slot(canvas, anatomy.frame, style.frame, slot,
                                    state.style_alpha);
        detail::paint_material_slot(canvas, anatomy.arrow, style.arrow_region,
                                    slot, state.style_alpha);
    }
    channels.merge();
    return popup_open;
}

/* Decorates one TreeNodeEx row. selected and leaf must match its flags.
 * Non-leaves must use SpanAvailWidth | OpenOnArrow; leaves must additionally
 * use Leaf | NoTreePushOnOpen. Parent channels are merged before callers draw
 * children. When a non-leaf returns true, the caller remains responsible for
 * TreePop after drawing those children. */
template <typename Widget>
bool decorate_tree_node(Frame &frame, const TreeStyle &style, bool selected,
                        bool leaf, Widget &&widget) {
    bool open = false;
    return detail::item_paint(
        frame, detail::Decorator::Tree,
        [&open, &widget] {
            open = std::forward<Widget>(widget)();
            return open;
        },
        [&style, selected, leaf, &open](
            const detail::ItemState &state,
            const std::optional<ip_rect> &, auto &canvas) {
            const detail::TreeAnatomy anatomy = detail::tree_anatomy(
                detail::item_rect(state), leaf,
                ImGui::GetTreeNodeToLabelSpacing());
            const detail::StateColorSlot slot = detail::tree_state_color_slot(
                detail::tree_visual_state(state, selected, open));
            detail::paint_material_slot(canvas, anatomy.row, style.row, slot,
                                        state.style_alpha);
            if (anatomy.disclosure) {
                detail::paint_material_slot(canvas, *anatomy.disclosure,
                                            style.disclosure, slot,
                                            state.style_alpha);
            }
        });
}

} // namespace ip

#endif /* IMGUI_PAINTER_DECORATORS_H */
