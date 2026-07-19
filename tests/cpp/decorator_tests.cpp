#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_painter_decorators.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr ip_color rgba(unsigned r, unsigned g, unsigned b, unsigned a = 255u) {
    return r | (g << 8u) | (b << 16u) | (a << 24u);
}

const ip::Material material{
    2.0f,
    {rgba(20, 30, 40), rgba(30, 40, 50), rgba(40, 50, 60)},
    {1.0f, rgba(255, 255, 255, 30)},
    std::nullopt,
};

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void initialize_context() {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    require(pixels != nullptr && width > 0 && height > 0,
            "font atlas failed to build");
}

void begin_fixed_window(const char *name) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 120.0f), ImGuiCond_Always);
    ImGui::Begin(name, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
}

bool same_vec(ImVec2 a, ImVec2 b) { return a.x == b.x && a.y == b.y; }

template <std::size_t Count>
void require_same_colors(ip::detail::Decorator decorator,
                         const ImGuiCol (&expected)[Count], const char *message) {
    const ip::detail::SuppressedColors actual =
        ip::detail::suppressed_colors(decorator);
    require(actual.count == static_cast<int>(Count), message);
    for (int index = 0; index < actual.count; ++index) {
        require(actual.data[index] == expected[index], message);
    }
}

void decorators_suppress_expected_color_families() {
    constexpr ImGuiCol button[] = {
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive};
    constexpr ImGuiCol header[] = {
        ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive};
    constexpr ImGuiCol frame[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive};
    constexpr ImGuiCol slider[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};

    require_same_colors(ip::detail::Decorator::Button, button,
                        "Button suppression family is wrong");
    require_same_colors(ip::detail::Decorator::Selectable, header,
                        "Selectable suppression family is wrong");
    require_same_colors(ip::detail::Decorator::Checkbox, frame,
                        "Checkbox suppression family is wrong");
    require_same_colors(ip::detail::Decorator::InputText, frame,
                        "InputText suppression family is wrong");
    require_same_colors(ip::detail::Decorator::Slider, slider,
                        "Slider suppression family is wrong");
}

bool same_rect(ip_rect a, ip_rect b) {
    return a.min.x == b.min.x && a.min.y == b.min.y &&
           a.max.x == b.max.x && a.max.y == b.max.y;
}

float center_x(ip_rect rect) { return (rect.min.x + rect.max.x) * 0.5f; }

void slider_grab_padding_ignores_framebuffer_scale() {
    const ip_rect frame{{0.0f, 0.0f}, {100.0f, 20.0f}};
    const ip_rect at_one =
        ip::detail::slider_anatomy(frame, 0.0f, 1.0f, 0.0f, 10.0f, 1.0f).grab;
    const ip_rect at_one_and_half =
        ip::detail::slider_anatomy(frame, 0.0f, 1.0f, 0.0f, 10.0f, 1.5f).grab;
    const ip_rect at_two =
        ip::detail::slider_anatomy(frame, 0.0f, 1.0f, 0.0f, 10.0f, 2.0f).grab;
    require(same_rect(at_one, at_one_and_half),
            "Slider grab changed at framebuffer scale 1.5");
    require(same_rect(at_one, at_two),
            "Slider grab changed at framebuffer scale 2.0");
}

void slider_track_minimum_uses_framebuffer_scale_only() {
    const ip_rect frame{{0.0f, 0.0f}, {100.0f, 4.0f}};
    const ip_rect at_one =
        ip::detail::slider_anatomy(frame, 0.0f, 1.0f, 0.5f, 2.0f, 1.0f).track;
    const ip_rect at_two =
        ip::detail::slider_anatomy(frame, 0.0f, 1.0f, 0.5f, 2.0f, 2.0f).track;
    require(ip::detail::rect_height(at_one) == 2.0f,
            "Slider track minimum at scale 1.0 is not two pixels");
    require(ip::detail::rect_height(at_two) == 1.0f,
            "Slider track minimum at scale 2.0 is not one logical unit");
}

void slider_anatomy_scales_with_logical_style_metrics() {
    const ip::detail::SliderAnatomy base = ip::detail::slider_anatomy(
        {{0.0f, 0.0f}, {100.0f, 20.0f}}, 0.0f, 1.0f, 0.5f, 10.0f, 1.0f);
    const ip::detail::SliderAnatomy scaled = ip::detail::slider_anatomy(
        {{0.0f, 0.0f}, {200.0f, 40.0f}}, 0.0f, 1.0f, 0.5f, 20.0f, 1.0f);
    require(ip::detail::rect_width(scaled.grab) ==
                ip::detail::rect_width(base.grab) * 2.0f,
            "Slider grab width did not scale with GrabMinSize");
    require(center_x(scaled.grab) == 100.0f,
            "Scaled Slider grab is not centered at x=100");
}

void slider_anatomy_maps_values_and_degenerate_ranges() {
    const ip_rect frame{{0.0f, 0.0f}, {100.0f, 20.0f}};
    const auto center = [frame](float min, float max, float value) {
        return center_x(ip::detail::slider_anatomy(
                            frame, min, max, value, 10.0f, 1.0f)
                            .grab);
    };
    require(center(0.0f, 1.0f, 0.0f) < 10.0f,
            "Slider minimum value did not map near the left edge");
    require(center(0.0f, 1.0f, 0.5f) == 50.0f,
            "Slider midpoint did not map to x=50");
    require(center(0.0f, 1.0f, 1.0f) > 90.0f,
            "Slider maximum value did not map near the right edge");
    require(center(1.0f, 0.0f, 1.0f) < 10.0f,
            "Reversed Slider range did not map its minimum position left");
    require(std::isfinite(center(1.0f, 1.0f, 1.0f)),
            "Degenerate Slider range produced a non-finite center");
}

void slider_visual_states_map_to_material_slots() {
    using ip::detail::SliderVisualState;
    using ip::detail::StateColorSlot;
    require(ip::detail::slider_state_color_slot(SliderVisualState::Idle) ==
                StateColorSlot::Base,
            "Idle Slider state did not map to Base");
    require(ip::detail::slider_state_color_slot(SliderVisualState::Hovered) ==
                StateColorSlot::Hover,
            "Hovered Slider state did not map to Hover");
    require(ip::detail::slider_state_color_slot(SliderVisualState::Focused) ==
                StateColorSlot::Hover,
            "Focused Slider state did not map to Hover");
    require(ip::detail::slider_state_color_slot(SliderVisualState::Adjusting) ==
                StateColorSlot::Active,
            "Adjusting Slider state did not map to Active");
}

void require_chrome_matches(const ip_rect &chrome, ImVec2 expected_min,
                            float expected_width, float expected_height,
                            const char *widget_name) {
    require(chrome.min.x == expected_min.x && chrome.min.y == expected_min.y,
            widget_name);
    require(chrome.max.x == expected_min.x + expected_width,
            widget_name);
    require(chrome.max.y == expected_min.y + expected_height,
            widget_name);
}

void multipart_chrome_excludes_visible_labels() {
    initialize_context();
    ImGui::NewFrame();
    begin_fixed_window("multipart chrome geometry");

    bool checked = false;
    const ImVec2 checkbox_cursor = ImGui::GetCursorScreenPos();
    const float checkbox_side = ImGui::GetFrameHeight();
    const std::optional<ip_rect> checkbox_chrome =
        ip::detail::capture_chrome(ip::detail::Decorator::Checkbox);
    require(checkbox_chrome.has_value(), "Checkbox chrome was not captured");
    ImGui::Checkbox(
        "Checkbox with a deliberately long visible label outside the painted box",
        &checked);
    const ImVec2 checkbox_item_min = ImGui::GetItemRectMin();
    const ImVec2 checkbox_item_max = ImGui::GetItemRectMax();
    const ip_rect checkbox_item{{checkbox_item_min.x, checkbox_item_min.y},
                                {checkbox_item_max.x, checkbox_item_max.y}};
    require_chrome_matches(*checkbox_chrome, checkbox_cursor, checkbox_side,
                           checkbox_side, "Checkbox chrome formula is wrong");
    require(checkbox_item.max.x - checkbox_chrome->max.x > 50.0f,
            "Checkbox chrome did not strictly exclude its visible label");
    require(ip::detail::rect_contains(checkbox_item, *checkbox_chrome),
            "Checkbox item rectangle does not contain its chrome");

    char buffer[64] = "editable";
    const ImVec2 input_cursor = ImGui::GetCursorScreenPos();
    const float input_width = ImGui::CalcItemWidth();
    const float input_height = ImGui::GetFrameHeight();
    const std::optional<ip_rect> input_chrome =
        ip::detail::capture_chrome(ip::detail::Decorator::InputText);
    require(input_chrome.has_value(), "InputText chrome was not captured");
    ImGui::InputText(
        "InputText with a deliberately long visible label outside the painted frame",
        buffer, sizeof(buffer));
    const ImVec2 input_item_min = ImGui::GetItemRectMin();
    const ImVec2 input_item_max = ImGui::GetItemRectMax();
    const ip_rect input_item{{input_item_min.x, input_item_min.y},
                             {input_item_max.x, input_item_max.y}};
    require_chrome_matches(*input_chrome, input_cursor, input_width, input_height,
                           "InputText chrome formula is wrong");
    require(input_item.max.x - input_chrome->max.x > 50.0f,
            "InputText chrome did not strictly exclude its visible label");
    require(ip::detail::rect_contains(input_item, *input_chrome),
            "InputText item rectangle does not contain its chrome");

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
}

void decoration_preserves_last_item_queries() {
    initialize_context();
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 button_center{};

    io.AddMousePosEvent(-100.0f, -100.0f);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    ImGui::NewFrame();
    begin_fixed_window("last-item contract");
    ImGui::Button("Contract button");
    const ImVec2 first_min = ImGui::GetItemRectMin();
    const ImVec2 first_max = ImGui::GetItemRectMax();
    button_center = ImVec2((first_min.x + first_max.x) * 0.5f,
                           (first_min.y + first_max.y) * 0.5f);
    ImGui::End();
    ImGui::Render();

    // 1.91's input queue may preserve move/down ordering across frames;
    // establish hover before submitting the press event.
    io.AddMousePosEvent(button_center.x, button_center.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    ImGui::NewFrame();
    begin_fixed_window("last-item contract");
    ImGui::Button("Contract button");
    ImGui::End();
    ImGui::Render();

    io.AddMousePosEvent(button_center.x, button_center.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    ImGui::NewFrame();
    ip::Context painter;
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    auto frame = painter.begin_frame({uv.x, uv.y});
    begin_fixed_window("last-item contract");

    ImGuiID inside_id = 0;
    ImVec2 inside_min{};
    ImVec2 inside_max{};
    bool inside_hovered = false;
    bool inside_active = false;
    ip::decorate_button(frame, material, [&] {
        const bool clicked = ImGui::Button("Contract button");
        inside_id = ImGui::GetItemID();
        inside_min = ImGui::GetItemRectMin();
        inside_max = ImGui::GetItemRectMax();
        inside_hovered = ImGui::IsItemHovered();
        inside_active = ImGui::IsItemActive();
        return clicked;
    });

    require(ImGui::GetItemID() == inside_id, "last-item ID changed");
    require(same_vec(ImGui::GetItemRectMin(), inside_min),
            "last-item minimum changed");
    require(same_vec(ImGui::GetItemRectMax(), inside_max),
            "last-item maximum changed");
    require(ImGui::IsItemHovered() == inside_hovered,
            "last-item hover query changed");
    require(ImGui::IsItemActive() == inside_active,
            "last-item active query changed");
    require(inside_hovered, "test button was not hovered");
    require(inside_active, "test button was not active");

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
}

void exception_restores_style_colors_and_draw_channels() {
    initialize_context();
    ImGui::NewFrame();
    ip::Context painter;
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    auto frame = painter.begin_frame({uv.x, uv.y});
    begin_fixed_window("exception cleanup");

    const int before_depth = GImGui->ColorStack.Size;
    bool threw = false;
    try {
        ip::decorate_button(frame, material, []() -> bool {
            ImGui::Button("throw");
            throw std::runtime_error("intentional decorator unwind");
        });
    } catch (const std::runtime_error &) {
        threw = true;
    }
    require(threw, "widget callable did not throw");
    require(GImGui->ColorStack.Size == before_depth,
            "style-color stack depth was not restored");

    // A second split on this same draw list asserts inside Dear ImGui if the
    // first decorator left its channel split stranded.
    ip::decorate_button(frame, material,
                        [] { return ImGui::Button("after exception"); });

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
}

} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2, "expected one test selector");
        const std::string selector = argv[1];
        if (selector == "suppression") {
            decorators_suppress_expected_color_families();
        } else if (selector == "chrome-geometry") {
            multipart_chrome_excludes_visible_labels();
        } else if (selector == "last-item") {
            decoration_preserves_last_item_queries();
        } else if (selector == "exception-safety") {
            exception_restores_style_colors_and_draw_channels();
        } else if (selector == "slider-grab-padding") {
            slider_grab_padding_ignores_framebuffer_scale();
        } else if (selector == "slider-track-minimum") {
            slider_track_minimum_uses_framebuffer_scale_only();
        } else if (selector == "slider-logical-scale") {
            slider_anatomy_scales_with_logical_style_metrics();
        } else if (selector == "slider-value-mapping") {
            slider_anatomy_maps_values_and_degenerate_ranges();
        } else if (selector == "slider-slot-mapping") {
            slider_visual_states_map_to_material_slots();
        } else {
            throw std::runtime_error("unknown test selector");
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
