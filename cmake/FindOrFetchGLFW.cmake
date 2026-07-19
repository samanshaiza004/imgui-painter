#[[
This module respects an existing `glfw` target, then a caller-supplied
checkout, and only otherwise fetches GLFW. The fallback is pinned to the
3.4 release used by the C++ examples.
]]

if(TARGET glfw)
    return()
endif()

# Keep GLFW's ancillary targets and install rules out of imgui-painter's
# consumer build regardless of which source-tree path is selected.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

if(GLFW_DIR)
    add_subdirectory("${GLFW_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/glfw")
else()
    include(FetchContent)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()
