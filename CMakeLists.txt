cmake_minimum_required(VERSION 3.16)
project(bt3d_raylib_port C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Every platform ships optimized builds unless a build type is chosen explicitly.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BT3D_PLATFORM_SWITCH "Build using raylib-nx/libnx for Nintendo Switch" OFF)
option(BT3D_PLATFORM_WEB "Build using Emscripten/raylib for browsers" OFF)
option(BT3D_PLATFORM_ANDROID "Build for Android through raylib/NDK" OFF)
option(BT3D_STATIC_RAYLIB "Statically link raylib for a portable single-file desktop binary" OFF)
option(BT3D_FETCH_RAYLIB "Fetch and build raylib automatically during configure" OFF)
option(BT3D_BUILD_TESTS "Build focused regression tests" OFF)

include(cmake/sources.cmake)
include(cmake/desktop.cmake)
include(cmake/raylib.cmake)
include(cmake/switch.cmake)
include(cmake/web.cmake)

include(cmake/midi.cmake)

if (BT3D_PLATFORM_SWITCH)
    if (NOT DEFINED CMAKE_SYSTEM_NAME OR NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        message(WARNING "BT3D_PLATFORM_SWITCH is ON but CMAKE_SYSTEM_NAME is not NintendoSwitch. Use the libnx/raylib-nx Switch toolchain when configuring this build.")
    endif()
    set(BT3D_TARGET_NAME bt3d_raylib_nx)
elseif (BT3D_PLATFORM_WEB)
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR "BT3D_PLATFORM_WEB=ON requires configuring with Emscripten, e.g. emcmake cmake ...")
    endif()
    set(BT3D_TARGET_NAME bt3d_raylib_web)
elseif (BT3D_PLATFORM_ANDROID)
    set(BT3D_TARGET_NAME bt3d_raylib_android)
else()
    set(BT3D_TARGET_NAME bt3d_raylib)
endif()

set(BT3D_PLATFORM_SOURCES ${BT3D_SOURCES})
if (WIN32)
    enable_language(RC)
    list(APPEND BT3D_PLATFORM_SOURCES ${CMAKE_SOURCE_DIR}/bt3d.rc)
endif()
add_executable(${BT3D_TARGET_NAME} ${BT3D_PLATFORM_SOURCES})
bt3d_embed_menu_midi(${BT3D_TARGET_NAME} "${CMAKE_SOURCE_DIR}")
target_include_directories(${BT3D_TARGET_NAME} PRIVATE src)
target_compile_definitions(${BT3D_TARGET_NAME} PRIVATE
    $<$<BOOL:${BT3D_PLATFORM_SWITCH}>:BT3D_PLATFORM_SWITCH=1>
    $<$<BOOL:${BT3D_PLATFORM_WEB}>:BT3D_PLATFORM_WEB=1>
    $<$<BOOL:${BT3D_PLATFORM_ANDROID}>:BT3D_PLATFORM_ANDROID=1>
)

if (BT3D_PLATFORM_SWITCH)
    bt3d_configure_switch_target(${BT3D_TARGET_NAME})
    bt3d_configure_switch_packaging(${BT3D_TARGET_NAME})
elseif (BT3D_PLATFORM_WEB)
    bt3d_link_desktop_or_web_raylib(${BT3D_TARGET_NAME})
    bt3d_configure_web_target(${BT3D_TARGET_NAME})
else()
    bt3d_link_desktop_or_web_raylib(${BT3D_TARGET_NAME})
    bt3d_configure_desktop_target(${BT3D_TARGET_NAME})
endif()

if (BT3D_BUILD_TESTS)
    enable_testing()

    add_executable(test_datpack tests/test_datpack.c src/datpack.c)
    target_include_directories(test_datpack PRIVATE src)
    add_test(NAME test_datpack COMMAND test_datpack)

    add_executable(test_map242 tests/test_map242.c src/map242.c)
    target_include_directories(test_map242 PRIVATE src)
    if (NOT MSVC)
        target_link_libraries(test_map242 PRIVATE m)
    endif()
    add_test(NAME test_map242 COMMAND test_map242)
    # The headless integration harness uses POSIX temporary files.
    if (UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT BT3D_PLATFORM_SWITCH AND NOT BT3D_PLATFORM_WEB AND NOT BT3D_PLATFORM_ANDROID)
        # Exercise the real gameplay/save code without opening a window or audio device.
        set(BT3D_TEST_SOURCES ${BT3D_SOURCES})
        list(REMOVE_ITEM BT3D_TEST_SOURCES src/main.c)
        add_executable(test_gameplay tests/test_gameplay.c ${BT3D_TEST_SOURCES})
        add_dependencies(test_gameplay ${BT3D_TARGET_NAME}_embedded_midi)
        target_include_directories(test_gameplay PRIVATE src "${CMAKE_BINARY_DIR}/generated")
        bt3d_link_desktop_or_web_raylib(test_gameplay)
        bt3d_configure_desktop_target(test_gameplay)
        add_test(NAME test_gameplay COMMAND test_gameplay)
    endif()
endif()
