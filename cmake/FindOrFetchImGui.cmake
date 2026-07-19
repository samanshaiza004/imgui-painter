#[[
This module respects an existing `imgui` target, then a caller-supplied
checkout, and only otherwise fetches Dear ImGui. The fallback is pinned to
the plain (non-docking) 1.91.9b release: the pinned Rust binding depends on
plain `imgui-sys = "0.12"` with no `docking` feature enabled anywhere in
this workspace, so imgui-sys's build.rs compiles its `third-party/imgui-master`
tree (see its docking_enabled branch), not `third-party/imgui-docking` --
confirmed by reading imgui-sys's build.rs and Cargo.lock directly, and by
the non-docking `ImGuiCol_COUNT == 56` (no ImGuiCol_DockingPreview/
DockingEmptyBg) that the actually-compiled bindings.rs uses. Matching that
exactly, not the docking branch, is what keeps this C++ layer's ImGuiCol_
role coverage identical to what the Rust reference is verified against.
]]

if(TARGET imgui)
    return()
endif()

if(IMGUI_DIR)
    set(_imgui_source_dir "${IMGUI_DIR}")
else()
    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b # Commit: f5befd2d29e66809cd1110a152e375a7f1981f06
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)
    set(_imgui_source_dir "${imgui_SOURCE_DIR}")
endif()

add_library(imgui STATIC
    "${_imgui_source_dir}/imgui.cpp"
    "${_imgui_source_dir}/imgui_draw.cpp"
    "${_imgui_source_dir}/imgui_tables.cpp"
    "${_imgui_source_dir}/imgui_widgets.cpp"
)
target_include_directories(imgui PUBLIC "${_imgui_source_dir}")

# Documented for callers that need sources not built into the core target,
# notably Dear ImGui's optional backend translation units.
set(IMGUI_PAINTER_IMGUI_SOURCE_DIR "${_imgui_source_dir}")
