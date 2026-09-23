cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
project(rainbow-triangle LANGUAGES CXX)

# JadeFX's macOS error dialog is Objective-C++. Enable it here so a nested
# add_subdirectory does not have to turn the language on after this cache exists.
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Editors (clangd, VS Code) read this so they use the same include paths as the build.
# JadeFX lives outside this tree; without it, `#include "jadefx/jadefx.hpp"` looks missing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer a system GLFW (Homebrew, vcpkg, apt's libglfw3-dev). Otherwise download
# the pinned 3.5.1 source. Building that source on Linux needs the X11 and
# Wayland development packages, because GLFW 3.5 enables both backends.
find_package(glfw3 3.3 QUIET CONFIG)
if(NOT TARGET glfw)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 QUIET IMPORTED_TARGET glfw3)
    endif()
endif()

if(TARGET glfw)
    set(RAINBOW_GLFW_TARGET glfw)
elseif(TARGET PkgConfig::GLFW3)
    set(RAINBOW_GLFW_TARGET PkgConfig::GLFW3)
else()
    include(FetchContent)
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
        URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    set(RAINBOW_GLFW_TARGET glfw)
endif()

# MACOSX_BUNDLE is ignored on Windows and Linux, where this stays a normal executable.
add_executable(rainbow-triangle MACOSX_BUNDLE
    src/main.cpp
    src/gl.cpp
    src/ShaderFile.cpp
    src/Renderer.cpp
)

# JadeFX draws the FPS label into this window. Its own samples stay out of `all`.
set(JADEFX_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JadeFX_CPP" CACHE PATH "JadeFX_CPP source directory")
if(NOT EXISTS "${JADEFX_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JadeFX_CPP was not found at ${JADEFX_CPP_DIR}")
endif()
add_subdirectory("${JADEFX_CPP_DIR}" "${CMAKE_BINARY_DIR}/jadefx" EXCLUDE_FROM_ALL)

target_link_libraries(rainbow-triangle PRIVATE ${RAINBOW_GLFW_TARGET} jadefx)

if(MSVC)
    target_compile_options(rainbow-triangle PRIVATE /W4 /utf-8)
else()
    target_compile_options(rainbow-triangle PRIVATE -Wall -Wextra)
endif()

if(APPLE)
    set_target_properties(rainbow-triangle PROPERTIES
        OUTPUT_NAME "Rainbow Triangle"
        MACOSX_BUNDLE_BUNDLE_NAME "Rainbow Triangle"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.rainbow-triangle"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
    )

    # Copy the runtime shaders into the bundle, then sign it so Finder can launch it.
    add_custom_target(bundle-shaders ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:rainbow-triangle>/Resources/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_BUNDLE_CONTENT_DIR:rainbow-triangle>/Resources/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${JADEFX_CPP_DIR}/res/shaders"
            "$<TARGET_BUNDLE_CONTENT_DIR:rainbow-triangle>/Resources/shaders"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:rainbow-triangle>"
        DEPENDS rainbow-triangle
        VERBATIM
    )
else()
    # JadeFX loads shaders from shaders/ beside the executable.
    add_custom_command(TARGET rainbow-triangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:rainbow-triangle>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:rainbow-triangle>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${JADEFX_CPP_DIR}/res/shaders" "$<TARGET_FILE_DIR:rainbow-triangle>/shaders"
        VERBATIM
    )
endif()
