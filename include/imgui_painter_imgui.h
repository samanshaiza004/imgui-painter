/* Dear ImGui host-value sampling is opt-in so the core fluent header remains
 * usable with any draw list exposing ImGui's public primitive-writing API.
 * Include this header when a real Dear ImGui context owns the frame state.
 */
#ifndef IMGUI_PAINTER_IMGUI_H
#define IMGUI_PAINTER_IMGUI_H

#include "imgui.h"
#include "imgui_painter.h"
#include "imgui_painter_recipes.h"

namespace ip {

/* These host-value APIs are stable and long-standing, so this is a minimum
 * compatibility floor rather than an exact version pin. */
static_assert(IMGUI_VERSION_NUM >= 19191,
              "imgui-painter requires Dear ImGui 1.91.9b or newer");

/* Sample both host values once for this frame, matching the Rust binding's
 * begin-frame semantics. Context owns validation of the framebuffer scale. */
inline Frame begin_frame(Context &ctx) {
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    return ctx.begin_frame({uv.x, uv.y}, ImGui::GetIO().DisplayFramebufferScale.x);
}

inline void apply_imgui_colors(ImVec4 colors[], const Palette &palette) {
    /* colors must point at an array of at least ImGuiCol_COUNT ImVec4 --
     * e.g. ImGui::GetStyle().Colors, or a caller-owned ImVec4[ImGuiCol_COUNT]. */
    const ip_color transparent = 0u;
    const ip_color frame_hover = detail::mix(palette.surface_inset, palette.selection, 0.14f);
    const ip_color button_hover =
        detail::mix(palette.surface_raised, palette.selection, 0.16f);
    const ip_color header_hover =
        detail::mix(palette.surface_raised, palette.selection, 0.18f);
    const ip_color separator = detail::with_alpha(palette.border_dark, 128u);
    const auto set = [colors](ImGuiCol slot, ip_color color) {
        constexpr float scale = 1.0f / 255.0f;
        colors[slot] = ImVec4{
            static_cast<float>(detail::channel(color, 0u)) * scale,
            static_cast<float>(detail::channel(color, 8u)) * scale,
            static_cast<float>(detail::channel(color, 16u)) * scale,
            static_cast<float>(detail::channel(color, 24u)) * scale,
        };
    };

    set(ImGuiCol_Text, palette.text);
    set(ImGuiCol_TextDisabled, palette.text_muted);
    set(ImGuiCol_WindowBg, palette.surface);
    set(ImGuiCol_ChildBg, palette.surface);
    set(ImGuiCol_PopupBg, palette.surface_raised);
    set(ImGuiCol_Border, palette.border_dark);
    set(ImGuiCol_BorderShadow, transparent);
    set(ImGuiCol_FrameBg, palette.surface_inset);
    set(ImGuiCol_FrameBgHovered, frame_hover);
    set(ImGuiCol_FrameBgActive, detail::shade(palette.surface_inset, 0.08f));
    set(ImGuiCol_TitleBg, palette.surface);
    set(ImGuiCol_TitleBgActive, palette.surface_raised);
    set(ImGuiCol_TitleBgCollapsed, palette.surface);
    set(ImGuiCol_MenuBarBg, palette.surface_raised);
    set(ImGuiCol_ScrollbarBg, detail::with_alpha(palette.surface_inset, 180u));
    set(ImGuiCol_ScrollbarGrab, palette.border_dark);
    set(ImGuiCol_ScrollbarGrabHovered,
        detail::mix(palette.border_dark, palette.selection, 0.35f));
    set(ImGuiCol_ScrollbarGrabActive, palette.selection);
    set(ImGuiCol_CheckMark, palette.selection);
    set(ImGuiCol_SliderGrab, palette.surface_raised);
    set(ImGuiCol_SliderGrabActive, palette.selection);
    set(ImGuiCol_Button, palette.surface_raised);
    set(ImGuiCol_ButtonHovered, button_hover);
    set(ImGuiCol_ButtonActive, detail::shade(palette.surface_raised, 0.12f));
    set(ImGuiCol_Header, palette.surface_raised);
    set(ImGuiCol_HeaderHovered, header_hover);
    set(ImGuiCol_HeaderActive, palette.selection);
    set(ImGuiCol_Separator, separator);
    set(ImGuiCol_SeparatorHovered,
        detail::mix(palette.border_dark, palette.selection, 0.45f));
    set(ImGuiCol_SeparatorActive, palette.selection);
    set(ImGuiCol_ResizeGrip, detail::with_alpha(palette.border_dark, 72u));
    set(ImGuiCol_ResizeGripHovered, detail::with_alpha(palette.selection, 170u));
    set(ImGuiCol_ResizeGripActive, palette.selection);
    set(ImGuiCol_Tab, palette.surface);
    set(ImGuiCol_TabHovered, button_hover);
    set(ImGuiCol_TabSelected, palette.selection);
    set(ImGuiCol_TabSelectedOverline, detail::tint(palette.selection, 0.20f));
    set(ImGuiCol_TabDimmed, detail::shade(palette.surface, 0.04f));
    set(ImGuiCol_TabDimmedSelected, detail::mix(palette.surface, palette.selection, 0.45f));
    set(ImGuiCol_TabDimmedSelectedOverline,
        detail::with_alpha(palette.selection, 150u));

    // Semantic exceptions: these communicate data/action rather than chrome.
    set(ImGuiCol_PlotLines, palette.text_muted);
    set(ImGuiCol_PlotLinesHovered, palette.selection);
    set(ImGuiCol_PlotHistogram, palette.accent);
    set(ImGuiCol_PlotHistogramHovered, detail::tint(palette.accent, 0.12f));
    set(ImGuiCol_TableHeaderBg, palette.surface_raised);
    set(ImGuiCol_TableBorderStrong, palette.border_dark);
    set(ImGuiCol_TableBorderLight, detail::with_alpha(palette.border_dark, 112u));
    set(ImGuiCol_TableRowBg, transparent);
    set(ImGuiCol_TableRowBgAlt, detail::with_alpha(palette.surface_raised, 96u));
    set(ImGuiCol_TextLink, palette.selection);
    set(ImGuiCol_TextSelectedBg, detail::with_alpha(palette.selection, 96u));
    set(ImGuiCol_DragDropTarget, palette.accent);
    set(ImGuiCol_NavCursor, detail::with_alpha(palette.selection, 210u));
    set(ImGuiCol_NavWindowingHighlight, detail::with_alpha(palette.border_light, 220u));
    set(ImGuiCol_NavWindowingDimBg, detail::with_alpha(palette.text, 48u));
    set(ImGuiCol_ModalWindowDimBg, detail::with_alpha(palette.text, 76u));
}

/* Target the current window's draw list. This has GetWindowDrawList()'s own
 * precondition: call it within the corresponding ImGui frame/window scope. */
inline auto window_canvas(Frame &frame) {
    return frame.canvas(*ImGui::GetWindowDrawList());
}

} // namespace ip

#endif /* IMGUI_PAINTER_IMGUI_H */
