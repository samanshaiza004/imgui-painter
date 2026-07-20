function(ip_apply_test_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

function(ip_add_registry_guard target selectors)
    add_test(NAME ${target}_registry_matches_cmake
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_EXECUTABLE=$<TARGET_FILE:${target}>"
            "-DEXPECTED_TESTS_CSV=$<JOIN:${selectors},,>"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_test_registry.cmake)
endfunction()

if(IMGUI_PAINTER_BUILD_TESTS)
    add_executable(imgui_painter_geometry_tests geometry_tests.cpp)
    target_compile_features(imgui_painter_geometry_tests PRIVATE cxx_std_17)
    target_link_libraries(imgui_painter_geometry_tests PRIVATE
        imgui_painter::imgui_painter)
    ip_apply_test_warnings(imgui_painter_geometry_tests)

    set(core_test_names
        version_is_reachable_through_ffi
        rgba_packs_r_in_the_lowest_byte
        radius_zero_never_produces_nan_positions
        solid_fill_produces_a_closed_triangle_fan
        no_shape_means_no_output
        begin_clears_the_previous_session
        zero_radius_still_fills_a_plain_rect
        larger_radius_tessellates_more_vertices_than_smaller
        segment_count_grows_sublinearly_with_radius
        linear_gradient_endpoints_hit_exact_stop_colors
        degenerate_gradient_axis_falls_back_to_first_stop
        degenerate_axis_falls_back_to_first_stop_for_angular_and_diamond
        angular_gradient_sweeps_from_the_to_direction
        angular_gradient_seam_is_a_hard_wrap_not_a_blend
        diamond_gradient_uses_per_axis_max_norm
        degenerate_zero_size_rect_does_not_panic
        empty_stops_is_a_no_op
        single_stop_gradient_fills_solid
        gradient_fill_tessellates_more_vertices_than_solid_fill
        radial_gradient_center_and_far_corner_hit_stop_colors
        band_fill_clips_vertices_and_accepts_inverted_endpoints
        band_outside_shape_is_empty_and_full_height_matches_fill_bounds
        gradient_band_clips_to_the_requested_interval
        inset_shadow_stays_inside_and_hard_band_reaches_only_spread
        blurred_offset_inset_shadow_stays_inside_shape
        inset_shadow_without_shape_emits_nothing
        hard_edged_shadow_is_a_single_uniform_ring
        blurred_shadow_produces_more_vertices_and_varied_alpha_than_hard_edged
        shadow_offset_translates_the_ring
        stacked_shadows_accumulate_more_vertices_than_one
        border_is_a_hollow_ring_with_exact_vertex_and_index_counts
        hairline_border_scales_alpha_instead_of_geometry
        pixel_scale_makes_half_logical_pixel_a_crisp_device_pixel
        inset_borders_stack_on_distinct_outlines
        invalid_border_geometry_is_ignored
        begin_resets_pixel_scale
        border_thicker_than_shape_does_not_panic
        no_shape_means_no_shadow_or_border_output
        line_produces_a_one_pixel_wide_quad_with_the_expected_span
        multiple_shapes_accumulate_into_one_mesh
    )
    foreach(test_name IN LISTS core_test_names)
        add_test(NAME ${test_name}
            COMMAND imgui_painter_geometry_tests ${test_name})
    endforeach()
    ip_add_registry_guard(imgui_painter_geometry_tests "${core_test_names}")
endif()

if(IMGUI_PAINTER_BUILD_IMGUI_TESTS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindOrFetchImGui.cmake")

    add_executable(imgui_painter_decorator_tests decorator_tests.cpp)
    target_compile_features(imgui_painter_decorator_tests PRIVATE cxx_std_17)
    target_link_libraries(imgui_painter_decorator_tests PRIVATE
        imgui_painter::imgui_painter
        imgui
    )
    ip_apply_test_warnings(imgui_painter_decorator_tests)

    # One add_test per test function, so a failure names the thing that actually
    # broke. Each runs in its own process, which also keeps ImGui's process-global
    # context from being shared across cases.
    set(decorator_tests
        "decorators_suppress_expected_color_families|suppression"
        "multipart_chrome_excludes_visible_labels|chrome-geometry"
        "decoration_preserves_last_item_queries|last-item"
        "exception_restores_style_colors_and_draw_channels|exception-safety"
        "slider_grab_padding_ignores_framebuffer_scale|slider-grab-padding"
        "slider_track_minimum_uses_framebuffer_scale_only|slider-track-minimum"
        "slider_anatomy_scales_with_logical_style_metrics|slider-logical-scale"
        "slider_anatomy_maps_values_and_degenerate_ranges|slider-value-mapping"
        "slider_visual_states_map_to_material_slots|slider-slot-mapping"
        "combo_anatomy_partitions_without_gap_or_overlap|combo-anatomy"
        "tree_anatomy_handles_leaf_and_clamps_disclosure|tree-anatomy"
        "combo_visual_states_map_to_material_slots|combo-slot-mapping"
        "tree_visual_states_map_to_material_slots|tree-slot-mapping"
        "combo_restores_parent_colors_before_popup_contents|combo-color-ordering"
    )
    set(decorator_test_selectors)
    foreach(test_entry IN LISTS decorator_tests)
        string(REPLACE "|" ";" test_parts "${test_entry}")
        list(GET test_parts 0 test_name)
        list(GET test_parts 1 test_selector)
        add_test(NAME ${test_name}
            COMMAND imgui_painter_decorator_tests ${test_selector})
        list(APPEND decorator_test_selectors ${test_selector})
    endforeach()
    ip_add_registry_guard(imgui_painter_decorator_tests
        "${decorator_test_selectors}")
endif()
