#ifndef IMGUI_PAINTER_EXAMPLES_BACKEND_H
#define IMGUI_PAINTER_EXAMPLES_BACKEND_H

#include <functional>

struct GLFWwindow;

namespace demo {

class Backend {
public:
    Backend(const char *title, int width = 1000, int height = 700);
    ~Backend();

    Backend(const Backend &) = delete;
    Backend &operator=(const Backend &) = delete;

    int run(const std::function<void()> &draw);
    float logical_ui_scale() const { return logical_ui_scale_; }
    float hidpi_scale() const { return hidpi_scale_; }

private:
    void begin_frame();
    void end_frame();

    GLFWwindow *window_ = nullptr;
    float logical_ui_scale_ = 1.0f;
    float hidpi_scale_ = 1.0f;
};

} // namespace demo

#endif
