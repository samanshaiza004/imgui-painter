#[[
This module respects an existing `imgui` target, then a caller-supplied
checkout, and only otherwise fetches Dear ImGui. The fallback is pinned to
the 1.91.9b docking release that matches imgui-painter's host binding.
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
        GIT_TAG v1.91.9b-docking # Peeled commit: 4806a1924ff6181180bf5e4b8b79ab4394118875
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

unset(_imgui_source_dir)
