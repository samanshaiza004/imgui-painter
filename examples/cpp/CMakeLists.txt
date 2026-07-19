include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindOrFetchImGui.cmake")

if(NOT IMGUI_PAINTER_IMGUI_SOURCE_DIR)
    message(FATAL_ERROR
        "The C++ examples need Dear ImGui backend sources, but an existing "
        "'imgui' target did not provide a source directory. Configure the "
        "examples without a pre-existing imgui target, or set IMGUI_DIR to "
        "a full upstream Dear ImGui checkout before imgui is created.")
endif()

set(_imgui_glfw_backend
    "${IMGUI_PAINTER_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
set(_imgui_gl3_backend
    "${IMGUI_PAINTER_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
if(NOT EXISTS "${_imgui_glfw_backend}" OR NOT EXISTS "${_imgui_gl3_backend}")
    message(FATAL_ERROR
        "Dear ImGui source directory '${IMGUI_PAINTER_IMGUI_SOURCE_DIR}' has "
        "no GLFW/OpenGL3 backends. imgui-sys's vendored tree strips the "
        "backends; IMGUI_DIR must name a full upstream Dear ImGui checkout.")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindOrFetchGLFW.cmake")
find_package(OpenGL REQUIRED)

add_library(imgui_backend_glfw_gl3 STATIC
    "${_imgui_glfw_backend}"
    "${_imgui_gl3_backend}"
)
target_link_libraries(imgui_backend_glfw_gl3 PUBLIC imgui glfw OpenGL::GL)
target_include_directories(imgui_backend_glfw_gl3 PUBLIC
    "${IMGUI_PAINTER_IMGUI_SOURCE_DIR}/backends"
)
if(APPLE)
    target_compile_definitions(imgui_backend_glfw_gl3 PUBLIC GL_SILENCE_DEPRECATION)
endif()

add_library(imgui_painter_demo_backend STATIC common/backend.cpp)
target_compile_features(imgui_painter_demo_backend PUBLIC cxx_std_17)
target_include_directories(imgui_painter_demo_backend PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
)
target_link_libraries(imgui_painter_demo_backend PUBLIC imgui_backend_glfw_gl3)

if(MSVC)
    target_compile_options(imgui_painter_demo_backend PRIVATE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui_painter_demo_backend PRIVATE -Wall -Wextra)
endif()

function(imgui_painter_add_example target source)
    add_executable(${target} "${source}")
    target_compile_features(${target} PRIVATE cxx_std_17)
    # imgui_painter_demo_backend already carries imgui (and GLFW/OpenGL)
    # transitively; naming imgui again here made the linker warn about a
    # duplicate libimgui.a on every example link.
    target_link_libraries(${target} PRIVATE
        imgui_painter::imgui_painter
        imgui_painter_demo_backend
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

imgui_painter_add_example(imgui_painter_basic basic/main.cpp)
imgui_painter_add_example(imgui_painter_gallery gallery/main.cpp)

unset(_imgui_glfw_backend)
unset(_imgui_gl3_backend)
