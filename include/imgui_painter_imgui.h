/* Dear ImGui host-value sampling is opt-in so the core fluent header remains
 * usable with any draw list exposing ImGui's public primitive-writing API.
 * Include this header when a real Dear ImGui context owns the frame state.
 */
#ifndef IMGUI_PAINTER_IMGUI_H
#define IMGUI_PAINTER_IMGUI_H

#include "imgui.h"
#include "imgui_painter.h"

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

/* Target the current window's draw list. This has GetWindowDrawList()'s own
 * precondition: call it within the corresponding ImGui frame/window scope. */
inline auto window_canvas(Frame &frame) {
    return frame.canvas(*ImGui::GetWindowDrawList());
}

} // namespace ip

#endif /* IMGUI_PAINTER_IMGUI_H */
