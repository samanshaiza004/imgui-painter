#include "backend.h"

#include "imgui.h"
#include "imgui_painter_decorators.h"
#include "imgui_painter_imgui.h"

#include <array>
#include <exception>
#include <iostream>

namespace {

constexpr ip_color rgba(unsigned r, unsigned g, unsigned b, unsigned a = 255u) {
    return r | (g << 8u) | (b << 16u) | (a << 24u);
}

const ip::Palette palette{
    rgba(61, 73, 82),       rgba(83, 99, 110),      rgba(34, 43, 49),
    rgba(132, 151, 163, 155), rgba(16, 22, 26, 235), rgba(72, 173, 222),
    rgba(54, 116, 151),     rgba(231, 238, 242),    rgba(158, 174, 184),
};

ip_rect rect(ImVec2 pos, float width, float height) {
    return {{pos.x, pos.y}, {pos.x + width, pos.y + height}};
}

void begin_section(const char *title) {
    ImGui::Spacing();
    ImGui::SeparatorText(title);
    ImGui::Spacing();
}

void draw_scale_status(const demo::Backend &backend) {
    begin_section("Scale diagnostics");
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("IMGUI_PAINTER_DEMO_UI_SCALE: %.2f", backend.logical_ui_scale());
    ImGui::Text("Startup framebuffer/window ratio: %.2f", backend.hidpi_scale());
    ImGui::Text("Live io.DisplayFramebufferScale: %.2f x %.2f",
                io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    ImGui::TextDisabled("Logical scale changes font/style metrics only; framebuffer scale stays physical.");
}

void draw_panel_recipes(ip::Frame &frame) {
    begin_section("Panel recipes");
    ImGui::TextDisabled("panel and inset_panel at application-sized geometry");
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ip_rect outer = rect(origin, 620.0f, 158.0f);
    {
        auto canvas = ip::window_canvas(frame);
        ip::panel(canvas, outer, palette);
    }
    const ImVec2 inset_origin{origin.x + 24.0f, origin.y + 40.0f};
    {
        auto canvas = ip::window_canvas(frame);
        ip::inset_panel(canvas, rect(inset_origin, 572.0f, 92.0f), palette);
    }
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(inset_origin.x + 18.0f, inset_origin.y + 18.0f), palette.text,
        "Inset content well");
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(inset_origin.x + 18.0f, inset_origin.y + 48.0f), palette.text_muted,
        "Hairlines continue to follow the live framebuffer scale.");
    ImGui::Dummy(ImVec2(640.0f, 178.0f));
}

// Gallery-only helper: Phase 6 can replace these static swatches with live
// decorators without adding example concerns to the public recipe headers.
void draw_material_swatches(ip::Frame &frame, const char *name,
                            const ip::Material &material) {
    ImGui::TextUnformatted(name);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const std::array<const char *, 3> labels{"base", "hover", "active"};
    const std::array<ip_color, 3> colors{
        material.fill.base, material.fill.hover, material.fill.active};
    constexpr float width = 132.0f;
    constexpr float height = 38.0f;
    constexpr float gap = 12.0f;

    for (std::size_t index = 0; index < colors.size(); ++index) {
        const ImVec2 pos{origin.x + static_cast<float>(index) * (width + gap), origin.y};
        {
            auto canvas = ip::window_canvas(frame);
            canvas.rounded_rect(rect(pos, width, height), material.radius);
            if (material.shadow) {
                canvas.shadow(*material.shadow);
            }
            canvas.fill(colors[index]).border(material.border);
        }
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(pos.x + 12.0f, pos.y + 10.0f), palette.text, labels[index]);
    }
    ImGui::Dummy(ImVec2(width * 3.0f + gap * 2.0f, height + 10.0f));
}

void draw_material_recipes(ip::Frame &frame) {
    begin_section("Material recipe state swatches");
    ImGui::TextDisabled("Every material component shows base / hover / active recipe data.");

    draw_material_swatches(frame, "raised_button", ip::raised_button(palette));
    draw_material_swatches(frame, "toolbar_button", ip::toolbar_button(palette));
    draw_material_swatches(frame, "inset_control", ip::inset_control(palette));
    draw_material_swatches(frame, "selected_row", ip::selected_row(palette));

    const ip::SliderStyle slider = ip::parameter_slider(palette);
    draw_material_swatches(frame, "parameter_slider.track", slider.track);
    draw_material_swatches(frame, "parameter_slider.fill", slider.fill);
    draw_material_swatches(frame, "parameter_slider.grab", slider.grab);

    const ip::ComboStyle combo = ip::combo_field(palette);
    draw_material_swatches(frame, "combo_field.frame", combo.frame);
    draw_material_swatches(frame, "combo_field.arrow_region", combo.arrow_region);

    const ip::TreeStyle tree = ip::browser_tree_row(palette);
    draw_material_swatches(frame, "browser_tree_row.row", tree.row);
    draw_material_swatches(frame, "browser_tree_row.disclosure", tree.disclosure);
}

void draw_host_chrome() {
    begin_section("Host ImGui chrome");
    ImGui::TextDisabled("The same Palette is applied to stock ImGui style colors.");
    static bool enabled = true;
    static float amount = 0.62f;
    static int mode = 1;
    ImGui::Checkbox("Stock checkbox", &enabled);
    ImGui::SliderFloat("Stock slider", &amount, 0.0f, 1.0f);
    ImGui::Combo("Stock combo", &mode, "Clean\0Warm\0Bright\0");
    ImGui::Button("Stock button");
}

void draw_widget_decorators(ip::Frame &frame) {
    begin_section("Stock widget decorators");
    ImGui::TextDisabled("Real ImGui widgets with imgui-painter chrome underneath their text.");

    const ip::Material button = ip::raised_button(palette);
    ip::decorate_button(frame, button, [] { return ImGui::Button("Decorated button"); });

    static bool first_selected = true;
    static bool second_selected = false;
    const ip::Material row = ip::selected_row(palette);
    if (ip::decorate_selectable(frame, row, first_selected, [&] {
            return ImGui::Selectable("Persistently selected row", first_selected);
        })) {
        first_selected = !first_selected;
    }
    if (ip::decorate_selectable(frame, row, second_selected, [&] {
            return ImGui::Selectable("Selectable row", second_selected);
        })) {
        second_selected = !second_selected;
    }

    const ip::Material inset = ip::inset_control(palette);
    static bool decorated_checkbox = true;
    ip::decorate_checkbox(frame, inset, [&] {
        return ImGui::Checkbox(
            "Decorated checkbox with a long label outside the painted box",
            &decorated_checkbox);
    });

    static char input_buffer[128] = "Editable stock ImGui text";
    ip::decorate_input_text(frame, inset, [&] {
        return ImGui::InputTextWithHint(
            "Decorated InputText with a long label outside the painted frame",
            "Type into the stock ImGui editor", input_buffer,
            sizeof(input_buffer));
    });

    const ip::SliderStyle slider = ip::parameter_slider(palette);
    static float gain = 0.62f;
    ip::decorate_slider_f32(frame, slider, 0.0f, 1.0f, gain, [&](float &value) {
        return ImGui::SliderFloat("Decorated gain", &value, 0.0f, 1.0f);
    });

    static float disabled_gain = 0.35f;
    ImGui::BeginDisabled();
    ip::decorate_slider_f32(
        frame, slider, 0.0f, 1.0f, disabled_gain, [&](float &value) {
            return ImGui::SliderFloat("Disabled decorated gain", &value, 0.0f,
                                      1.0f);
        });
    ImGui::EndDisabled();
}

void draw_gallery(ip::Context &context, const demo::Backend &backend) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("imgui-painter gallery", nullptr, flags);
    ImGui::TextUnformatted("imgui-painter C++ gallery — Phase 6 visual gate");

    auto frame = ip::begin_frame(context);
    draw_scale_status(backend);
    draw_panel_recipes(frame);
    draw_material_recipes(frame);
    draw_widget_decorators(frame);
    draw_host_chrome();
    ImGui::End();
}

} // namespace

int main() {
    try {
        demo::Backend backend("imgui-painter C++ gallery", 1100, 800);
        ip::apply_imgui_colors(ImGui::GetStyle().Colors, palette);
        ip::Context context;
        bool reported_scale = false;
        return backend.run([&] {
            if (!reported_scale) {
                const ImGuiIO &io = ImGui::GetIO();
                std::cout << "logical_ui_scale=" << backend.logical_ui_scale()
                          << " display_framebuffer_scale="
                          << io.DisplayFramebufferScale.x << 'x'
                          << io.DisplayFramebufferScale.y << '\n';
                reported_scale = true;
            }
            draw_gallery(context, backend);
        });
    } catch (const std::exception &error) {
        std::cerr << "imgui_painter_gallery: " << error.what() << '\n';
        return 1;
    }
}
