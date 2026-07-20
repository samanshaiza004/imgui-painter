#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_painter_decorators.h"
#include "test_harness.h"

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

using ip_test::require;

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

IP_TEST_CASE(decorators_suppress_expected_color_families, "suppression") {
    constexpr ImGuiCol button[] = {
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive};
    constexpr ImGuiCol header[] = {
        ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive};
    constexpr ImGuiCol frame[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive};
    constexpr ImGuiCol slider[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
    constexpr ImGuiCol combo[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive};

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
    require_same_colors(ip::detail::Decorator::Combo, combo,
                        "Combo suppression family is wrong");
    require_same_colors(ip::detail::Decorator::Tree, header,
                        "Tree suppression family is wrong");
}

bool same_rect(ip_rect a, ip_rect b) {
    return a.min.x == b.min.x && a.min.y == b.min.y &&
           a.max.x == b.max.x && a.max.y == b.max.y;
}

float center_x(ip_rect rect) { return (rect.min.x + rect.max.x) * 0.5f; }

IP_TEST_CASE(combo_anatomy_partitions_without_gap_or_overlap, "combo-anatomy") {
    const ip_rect frame{{2.0f, 3.0f}, {102.0f, 23.0f}};
    const ip::detail::ComboAnatomy anatomy =
        ip::detail::combo_anatomy(frame);
    require(anatomy.preview.max.x == anatomy.arrow.min.x,
            "Combo preview and arrow do not share a boundary");
    require(ip::detail::rect_width(anatomy.arrow) == 20.0f,
            "Combo arrow is not square");
    require(ip::detail::rect_contains(frame, anatomy.preview),
            "Combo preview escaped the frame");
    require(ip::detail::rect_contains(frame, anatomy.arrow),
            "Combo arrow escaped the frame");

    const ip_rect narrow{{4.0f, 5.0f}, {14.0f, 35.0f}};
    const ip::detail::ComboAnatomy narrow_anatomy =
        ip::detail::combo_anatomy(narrow);
    require(ip::detail::rect_width(narrow_anatomy.arrow) == 10.0f,
            "Narrow Combo arrow did not clamp to the frame width");
    require(narrow_anatomy.preview.max.x == narrow_anatomy.arrow.min.x,
            "Narrow Combo partition has a gap or overlap");
    require(ip::detail::rect_contains(narrow, narrow_anatomy.arrow),
            "Narrow Combo arrow escaped the frame");
}

IP_TEST_CASE(tree_anatomy_handles_leaf_and_clamps_disclosure, "tree-anatomy") {
    const ip_rect row{{10.0f, 20.0f}, {110.0f, 40.0f}};
    const ip::detail::TreeAnatomy leaf =
        ip::detail::tree_anatomy(row, true, 18.0f);
    require(!leaf.disclosure.has_value(),
            "Leaf TreeNode unexpectedly has disclosure chrome");

    const ip::detail::TreeAnatomy branch =
        ip::detail::tree_anatomy(row, false, 18.0f);
    require(branch.disclosure.has_value(),
            "Non-leaf TreeNode has no disclosure chrome");
    require(ip::detail::rect_contains(row, *branch.disclosure),
            "TreeNode disclosure escaped the row");

    const ip::detail::TreeAnatomy clamped =
        ip::detail::tree_anatomy(row, false, 10000.0f);
    require(clamped.disclosure->max.x == row.max.x,
            "TreeNode disclosure did not clamp to the row's right edge");
    require(ip::detail::rect_contains(row, *clamped.disclosure),
            "Clamped TreeNode disclosure escaped the row");
}

IP_TEST_CASE(combo_visual_states_map_to_material_slots, "combo-slot-mapping") {
    using ip::detail::ComboVisualState;
    using ip::detail::StateColorSlot;
    require(ip::detail::combo_state_color_slot(ComboVisualState::Idle) ==
                StateColorSlot::Base,
            "Idle Combo state did not map to Base");
    require(ip::detail::combo_state_color_slot(ComboVisualState::Hovered) ==
                StateColorSlot::Hover,
            "Hovered Combo state did not map to Hover");
    require(ip::detail::combo_state_color_slot(ComboVisualState::Focused) ==
                StateColorSlot::Hover,
            "Focused Combo state did not map to Hover");
    require(ip::detail::combo_state_color_slot(ComboVisualState::Pressed) ==
                StateColorSlot::Active,
            "Pressed Combo state did not map to Active");
    require(ip::detail::combo_state_color_slot(ComboVisualState::Open) ==
                StateColorSlot::Active,
            "Open Combo state did not map to Active");
}

IP_TEST_CASE(tree_visual_states_map_to_material_slots, "tree-slot-mapping") {
    using ip::detail::StateColorSlot;
    using ip::detail::TreeVisualState;
    require(ip::detail::tree_state_color_slot(TreeVisualState::Idle) ==
                StateColorSlot::Base,
            "Idle Tree state did not map to Base");
    require(ip::detail::tree_state_color_slot(TreeVisualState::Open) ==
                StateColorSlot::Base,
            "Open Tree state did not deliberately map to Base");
    require(ip::detail::tree_state_color_slot(TreeVisualState::Hovered) ==
                StateColorSlot::Hover,
            "Hovered Tree state did not map to Hover");
    require(ip::detail::tree_state_color_slot(TreeVisualState::Focused) ==
                StateColorSlot::Hover,
            "Focused Tree state did not map to Hover");
    require(ip::detail::tree_state_color_slot(TreeVisualState::Selected) ==
                StateColorSlot::Active,
            "Selected Tree state did not map to Active");
    require(ip::detail::tree_state_color_slot(TreeVisualState::Pressed) ==
                StateColorSlot::Active,
            "Pressed Tree state did not map to Active");
}

IP_TEST_CASE(slider_grab_padding_ignores_framebuffer_scale, "slider-grab-padding") {
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

IP_TEST_CASE(slider_track_minimum_uses_framebuffer_scale_only, "slider-track-minimum") {
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

IP_TEST_CASE(slider_anatomy_scales_with_logical_style_metrics, "slider-logical-scale") {
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

IP_TEST_CASE(slider_anatomy_maps_values_and_degenerate_ranges, "slider-value-mapping") {
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

IP_TEST_CASE(slider_visual_states_map_to_material_slots, "slider-slot-mapping") {
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

IP_TEST_CASE(multipart_chrome_excludes_visible_labels, "chrome-geometry") {
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

IP_TEST_CASE(decoration_preserves_last_item_queries, "last-item") {
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

IP_TEST_CASE(exception_restores_style_colors_and_draw_channels, "exception-safety") {
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

IP_TEST_CASE(combo_restores_parent_colors_before_popup_contents, "combo-color-ordering") {
    initialize_context();
    ImGui::NewFrame();
    ip::Context painter;
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    auto frame = painter.begin_frame({uv.x, uv.y});
    begin_fixed_window("combo color restoration");

    constexpr const char *label = "Mode";
    const ImGuiID combo_id = ImGui::GetCurrentWindow()->GetID(label);
    ImGui::OpenPopupEx(ImHashStr("##ComboPopup", 0, combo_id),
                       ImGuiPopupFlags_None);
    const int before_depth = GImGui->ColorStack.Size;
    const ImVec4 before_frame_bg =
        ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    bool contents_ran = false;
    const ip::ComboStyle style{material, material};
    const bool popup_open = ip::decorate_combo(
        frame, style, [&] { return ImGui::BeginCombo(label, "Clean"); }, [&] {
            contents_ran = true;
            require(GImGui->ColorStack.Size == before_depth,
                    "Combo colors remained pushed inside popup contents");
            const ImVec4 inside =
                ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
            require(inside.x == before_frame_bg.x &&
                        inside.y == before_frame_bg.y &&
                        inside.z == before_frame_bg.z &&
                        inside.w == before_frame_bg.w,
                    "Popup contents inherited transparent Combo colors");
        });
    require(popup_open, "Combo popup did not open for the ordering test");
    require(contents_ran, "Combo popup contents did not run");

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
}

} // namespace

int main(int argc, char **argv) {
    return ip_test::run(argc, argv);
}
