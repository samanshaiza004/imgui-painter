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

# One add_test per test function, so a failure names the thing that actually
# broke. Each runs in its own process, which also keeps ImGui's process-global
# context from being shared across cases.
add_test(NAME decorators_suppress_expected_color_families
    COMMAND imgui_painter_decorator_tests suppression)
add_test(NAME multipart_chrome_excludes_visible_labels
    COMMAND imgui_painter_decorator_tests chrome-geometry)
add_test(NAME decoration_preserves_last_item_queries
    COMMAND imgui_painter_decorator_tests last-item)
add_test(NAME exception_restores_style_colors_and_draw_channels
    COMMAND imgui_painter_decorator_tests exception-safety)
add_test(NAME slider_grab_padding_ignores_framebuffer_scale
    COMMAND imgui_painter_decorator_tests slider-grab-padding)
add_test(NAME slider_track_minimum_uses_framebuffer_scale_only
    COMMAND imgui_painter_decorator_tests slider-track-minimum)
add_test(NAME slider_anatomy_scales_with_logical_style_metrics
    COMMAND imgui_painter_decorator_tests slider-logical-scale)
add_test(NAME slider_anatomy_maps_values_and_degenerate_ranges
    COMMAND imgui_painter_decorator_tests slider-value-mapping)
add_test(NAME slider_visual_states_map_to_material_slots
    COMMAND imgui_painter_decorator_tests slider-slot-mapping)
add_test(NAME combo_anatomy_partitions_without_gap_or_overlap
    COMMAND imgui_painter_decorator_tests combo-anatomy)
add_test(NAME tree_anatomy_handles_leaf_and_clamps_disclosure
    COMMAND imgui_painter_decorator_tests tree-anatomy)
add_test(NAME combo_visual_states_map_to_material_slots
    COMMAND imgui_painter_decorator_tests combo-slot-mapping)
add_test(NAME tree_visual_states_map_to_material_slots
    COMMAND imgui_painter_decorator_tests tree-slot-mapping)
add_test(NAME combo_restores_parent_colors_before_popup_contents
    COMMAND imgui_painter_decorator_tests combo-color-ordering)
