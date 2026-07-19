include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindOrFetchImGui.cmake")

add_executable(imgui_painter_decorator_tests decorator_tests.cpp)
target_compile_features(imgui_painter_decorator_tests PRIVATE cxx_std_17)
target_link_libraries(imgui_painter_decorator_tests PRIVATE
    imgui_painter::imgui_painter
    imgui
)

if(MSVC)
    target_compile_options(imgui_painter_decorator_tests PRIVATE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui_painter_decorator_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME decoration_preserves_last_item_queries
    COMMAND imgui_painter_decorator_tests last-item)
add_test(NAME exception_restores_style_colors_and_draw_channels
    COMMAND imgui_painter_decorator_tests exception-safety)
