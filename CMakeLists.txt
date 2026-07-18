cmake_minimum_required(VERSION 3.16)

project(imgui_painter VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# PROJECT_IS_TOP_LEVEL is only set by project() on CMake >= 3.21. Deriving it
# here keeps the 3.16 minimum honest: without this, 3.16-3.20 would leave the
# variable undefined, silently defaulting both options below to OFF and
# dropping -Werror rather than failing loudly.
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL ON)
    else()
        set(PROJECT_IS_TOP_LEVEL OFF)
    endif()
endif()

option(IMGUI_PAINTER_BUILD_EXAMPLES "Build imgui-painter examples" ${PROJECT_IS_TOP_LEVEL})
option(IMGUI_PAINTER_BUILD_TESTS "Build imgui-painter tests" ${PROJECT_IS_TOP_LEVEL})

set(IMGUI_PAINTER_SOURCES
    src/painter.cpp
    capi/imgui_painter_c.cpp
)

add_library(imgui_painter ${IMGUI_PAINTER_SOURCES})
add_library(imgui_painter::imgui_painter ALIAS imgui_painter)

target_compile_features(imgui_painter PUBLIC cxx_std_17)
set_target_properties(imgui_painter PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(imgui_painter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/capi>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/imgui-painter>
)

if(MSVC)
    target_compile_options(imgui_painter PRIVATE /W4)
    if(PROJECT_IS_TOP_LEVEL)
        target_compile_options(imgui_painter PRIVATE /WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui_painter PRIVATE -Wall -Wextra)
    if(PROJECT_IS_TOP_LEVEL)
        target_compile_options(imgui_painter PRIVATE -Werror)
    endif()
endif()

install(TARGETS imgui_painter
    EXPORT imgui-painterTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# Deliberately no `INCLUDES DESTINATION` above: it would append the bare
# ${prefix}/include to the exported target's interface, putting the whole
# shared include dir on every consumer's search path (so an unrelated header
# in /usr/local/include could shadow theirs) and quietly blessing a second
# spelling, `#include <imgui_painter.h>`. The $<INSTALL_INTERFACE:> in
# target_include_directories is the only installed include dir we promise.

install(FILES
    include/imgui_painter.h
    capi/imgui_painter_c.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgui-painter
)

set(IMGUI_PAINTER_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/imgui-painter)

install(EXPORT imgui-painterTargets
    FILE imgui-painterTargets.cmake
    NAMESPACE imgui_painter::
    DESTINATION ${IMGUI_PAINTER_CMAKE_INSTALL_DIR}
)

configure_package_config_file(
    cmake/imgui-painterConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/imgui-painterConfig.cmake
    INSTALL_DESTINATION ${IMGUI_PAINTER_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/imgui-painterConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/imgui-painterConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/imgui-painterConfigVersion.cmake
    DESTINATION ${IMGUI_PAINTER_CMAKE_INSTALL_DIR}
)

export(EXPORT imgui-painterTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/imgui-painterTargets.cmake
    NAMESPACE imgui_painter::
)
