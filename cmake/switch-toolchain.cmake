# CMake toolchain file for Nintendo Switch (devkitPro / libnx)
#
# Usage:
#   cmake -S . -B build/switch \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/switch-toolchain.cmake \
#     -DBT3D_PLATFORM_SWITCH=ON
#
# Requires:
#   - DEVKITPRO environment variable (e.g. /opt/devkitpro)
#   - devkitA64 toolchain installed
#   - libnx installed
#   - raylib-nx built and installed to $DEVKITPRO/portlibs/switch

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable is not set. Install devkitPro and set it (e.g. export DEVKITPRO=/opt/devkitpro)")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)
set(LIBNX ${DEVKITPRO}/libnx)
set(PORTLIBS ${DEVKITPRO}/portlibs/switch)

set(CMAKE_SYSTEM_NAME NintendoSwitch)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler
set(CMAKE_C_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++)
set(CMAKE_AR ${DEVKITA64}/bin/aarch64-none-elf-ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB ${DEVKITA64}/bin/aarch64-none-elf-ranlib CACHE FILEPATH "Ranlib")

# Architecture flags matching raylib-nx Makefile
set(SWITCH_ARCH_FLAGS "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec")
set(SWITCH_COMMON_FLAGS "${SWITCH_ARCH_FLAGS} -ffunction-sections -fdata-sections -D__SWITCH__")

set(CMAKE_C_FLAGS_INIT "${SWITCH_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${SWITCH_COMMON_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-specs=${LIBNX}/switch.specs -L${LIBNX}/lib -L${PORTLIBS}/lib")

# Search paths
set(CMAKE_FIND_ROOT_PATH ${PORTLIBS} ${LIBNX} ${DEVKITA64})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Include paths
include_directories(${LIBNX}/include ${PORTLIBS}/include)
link_directories(${LIBNX}/lib ${PORTLIBS}/lib)
