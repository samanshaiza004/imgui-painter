#include "backend.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace demo {
namespace {

#if defined(__APPLE__)
constexpr const char *glsl_version = "#version 150";
#else
constexpr const char *glsl_version = "#version 130";
#endif

float read_logical_ui_scale() {
    const char *value = std::getenv("IMGUI_PAINTER_DEMO_UI_SCALE");
    if (value == nullptr) {
        return 1.0f;
    }

    try {
        std::size_t parsed = 0;
        const std::string text(value);
        const float scale = std::stof(text, &parsed);
        if (parsed == text.size() && std::isfinite(scale) && scale > 0.0f) {
            return scale;
        }
    } catch (const std::exception &) {
    }
    return 1.0f;
}

void glfw_error_callback(int error, const char *description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

float framebuffer_scale(GLFWwindow *window) {
    int window_width = 0;
    int window_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (window_width <= 0 || window_height <= 0 || framebuffer_width <= 0 ||
        framebuffer_height <= 0) {
        return 1.0f;
    }
    const float scale_x = static_cast<float>(framebuffer_width) /
                          static_cast<float>(window_width);
    return scale_x;
}

} // namespace

Backend::Backend(const char *title, int width, int height)
    : logical_ui_scale_(read_logical_ui_scale()) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        throw std::runtime_error("failed to initialize GLFW");
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("failed to create GLFW window");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // This is the physical-to-logical render-target ratio on Retina and is
    // the same quantity the GLFW backend reports as DisplayFramebufferScale.
    // The environment override never participates in this value.
    hidpi_scale_ = framebuffer_scale(window_);
    io.FontGlobalScale = 1.0f / hidpi_scale_;
    ImGui::GetStyle().ScaleAllSizes(logical_ui_scale_);
    ImFontConfig font_config;
    font_config.OversampleH = 1;
    font_config.PixelSnapH = true;
    font_config.SizePixels = 14.0f * hidpi_scale_ * logical_ui_scale_;
    io.Fonts->AddFontDefault(&font_config);

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("failed to initialize ImGui GLFW backend");
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("failed to initialize ImGui OpenGL3 backend");
    }
}

Backend::~Backend() {
    if (window_ == nullptr) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Backend::begin_frame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Backend::end_frame() {
    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.055f, 0.063f, 0.071f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

int Backend::run(const std::function<void()> &draw) {
    while (!glfwWindowShouldClose(window_)) {
        begin_frame();
        draw();
        end_frame();
    }
    return 0;
}

} // namespace demo
