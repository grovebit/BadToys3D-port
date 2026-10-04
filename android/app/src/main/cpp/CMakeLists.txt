cmake_minimum_required(VERSION 3.22)
project(bt3d_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(BT3D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

set(BT3D_PLATFORM_ANDROID ON CACHE BOOL "Build Android native library" FORCE)
set(PLATFORM Android CACHE STRING "raylib platform" FORCE)
set(GRAPHICS GRAPHICS_API_OPENGL_ES2 CACHE STRING "raylib graphics API" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib as a static dependency" FORCE)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

include("${BT3D_ROOT}/cmake/sources.cmake")
list(TRANSFORM BT3D_SOURCES PREPEND "${BT3D_ROOT}/")

include("${BT3D_ROOT}/cmake/midi.cmake")

add_library(main SHARED ${BT3D_SOURCES})
bt3d_embed_menu_midi(main "${BT3D_ROOT}")
target_include_directories(main PRIVATE "${BT3D_ROOT}/src")
target_compile_definitions(main PRIVATE BT3D_PLATFORM_ANDROID=1)
target_link_libraries(main PRIVATE raylib)
