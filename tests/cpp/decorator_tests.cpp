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
        if (selector == "last-item") {
            decoration_preserves_last_item_queries();
        } else if (selector == "exception-safety") {
            exception_restores_style_colors_and_draw_channels();
        } else {
            throw std::runtime_error("unknown test selector");
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
