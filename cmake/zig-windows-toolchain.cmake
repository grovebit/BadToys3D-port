# Cross-compile toolchain for 64-bit Windows binaries using Zig as the C/C++
# compiler driver. Requires `zig` and typically `ninja` on the host.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER zig)
set(CMAKE_C_COMPILER_ARG1 cc)
set(CMAKE_C_COMPILER_TARGET x86_64-windows-gnu)

set(CMAKE_CXX_COMPILER zig)
set(CMAKE_CXX_COMPILER_ARG1 c++)
set(CMAKE_CXX_COMPILER_TARGET x86_64-windows-gnu)

# Prefer env-overridable paths; fall back to homebrew llvm on mac, then the
# zig-ar / zig-ranlib wrappers that ship with this repo so the toolchain
# also works inside the docker build image (no homebrew available there).
if(NOT CMAKE_AR)
    find_program(CMAKE_AR NAMES llvm-ar
        HINTS /opt/homebrew/opt/llvm@21/bin /opt/homebrew/opt/llvm/bin /usr/bin /usr/local/bin)
endif()
if(NOT CMAKE_AR)
    set(CMAKE_AR "${CMAKE_CURRENT_LIST_DIR}/zig-ar")
endif()
if(NOT CMAKE_RANLIB)
    find_program(CMAKE_RANLIB NAMES llvm-ranlib
        HINTS /opt/homebrew/opt/llvm@21/bin /opt/homebrew/opt/llvm/bin /usr/bin /usr/local/bin)
endif()
if(NOT CMAKE_RANLIB)
    set(CMAKE_RANLIB "${CMAKE_CURRENT_LIST_DIR}/zig-ranlib")
endif()

find_program(BT3D_RC_WINDRES
    NAMES llvm-windres x86_64-w64-mingw32-windres
    HINTS /opt/homebrew/opt/llvm@21/bin /opt/homebrew/opt/llvm/bin /usr/bin /usr/local/bin)
if (BT3D_RC_WINDRES)
    set(CMAKE_RC_COMPILER "${BT3D_RC_WINDRES}")
    set(CMAKE_RC_FLAGS_INIT "--target=pe-x86-64")
endif()

# Prefer target libraries/packages over host ones while still allowing host
# tools such as cmake, ninja, and git to be found normally.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Produce a self-contained Windows executable where possible.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
