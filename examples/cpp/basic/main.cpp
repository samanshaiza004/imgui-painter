#include "backend.h"

#include "imgui.h"
#include "imgui_painter_imgui.h"

#include <exception>
#include <iostream>

namespace {

constexpr ip_color rgba(unsigned r, unsigned g, unsigned b, unsigned a = 255u) {
    return r | (g << 8u) | (b << 16u) | (a << 24u);
}

ip_rect rect(ImVec2 pos, float width, float height) {
    return {{pos.x, pos.y}, {pos.x + width, pos.y + height}};
}

void draw_basic(ip::Context &context) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("imgui-painter basic", nullptr, flags);
    ImGui::TextUnformatted("C++ fluent canvas — Context / Frame / Canvas");
    ImGui::TextDisabled("Gradient, bands, shadows, borders, and lines share a real ImDrawList.");
    ImGui::Spacing();

    auto frame = ip::begin_frame(context);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    const ip_color_stop blue_stops[] = {
        {0.0f, rgba(78, 156, 255)},
        {1.0f, rgba(28, 77, 176)},
    };
    const ip_gradient blue_gradient{
        IP_GRADIENT_LINEAR,
        {origin.x, origin.y},
        {origin.x, origin.y + 92.0f},
        blue_stops,
        2,
    };
    {
        auto canvas = ip::window_canvas(frame);
        canvas.rounded_rect(rect(origin, 280.0f, 92.0f), 10.0f)
            .shadow({{0.0f, 5.0f}, 14.0f, 1.0f, rgba(0, 0, 0, 115), false})
            .fill(blue_gradient)
            .band(origin.y + 1.0f, origin.y + 3.0f, rgba(255, 255, 255, 72))
            .border({1.0f, rgba(8, 30, 75, 235)})
            .line({origin.x + 24.0f, origin.y + 66.0f},
                  {origin.x + 256.0f, origin.y + 28.0f}, 2.0f,
                  rgba(220, 240, 255, 225));
    }

    const ImVec2 second{origin.x + 310.0f, origin.y};
    {
        auto canvas = ip::window_canvas(frame);
        canvas.rounded_rect(rect(second, 190.0f, 92.0f), 7.0f)
            .shadow({{0.0f, 2.0f}, 7.0f, 0.0f, rgba(0, 0, 0, 90), true})
            .fill(rgba(45, 53, 62))
            .band(second.y + 62.0f, second.y + 91.0f, rgba(22, 27, 32))
            .border({1.0f, rgba(130, 150, 164, 145)});
    }

    const ImVec2 third{origin.x, origin.y + 128.0f};
    {
        auto canvas = ip::window_canvas(frame);
        canvas.rounded_rect(rect(third, 500.0f, 44.0f), 5.0f)
            .fill(rgba(65, 76, 85))
            .band(third.y + 1.0f, third.y + 2.0f, rgba(255, 255, 255, 55))
            .border({1.0f, rgba(11, 16, 19, 230)})
            .line({third.x + 18.0f, third.y + 22.0f},
                  {third.x + 482.0f, third.y + 22.0f}, 1.0f,
                  rgba(92, 196, 235));
    }

    ImGui::Dummy(ImVec2(520.0f, 190.0f));
    ImGui::End();
}

} // namespace

int main() {
    try {
        demo::Backend backend("imgui-painter C++ basic");
        ip::Context context;
        return backend.run([&] { draw_basic(context); });
    } catch (const std::exception &error) {
        std::cerr << "imgui_painter_basic: " << error.what() << '\n';
        return 1;
    }
}
