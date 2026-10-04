# Unified build image for bt3d raylib-port Windows + Switch targets.
#
# - devkitpro/devkita64: devkitA64 toolchain + Switch libs
# - zig + llvm + mingw-w64: Windows cross-compile (via zig cc + windres)
# - raylib-nx prebuilt and installed into $DEVKITPRO/portlibs/switch
#
# macOS .app is NOT produced here (Apple SDK licensing blocks redistribution).
FROM devkitpro/devkita64:20260215

ARG ZIG_VERSION=0.13.0

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget xz-utils git cmake ninja-build make \
        llvm lld mingw-w64 rsync && \
    rm -rf /var/lib/apt/lists/*

# Install zig
RUN arch=$(uname -m) && \
    case "$arch" in \
        x86_64)  zdir=zig-linux-x86_64-${ZIG_VERSION} ;; \
        aarch64) zdir=zig-linux-aarch64-${ZIG_VERSION} ;; \
        *) echo "unsupported arch $arch" >&2; exit 1 ;; \
    esac && \
    wget -q "https://ziglang.org/download/${ZIG_VERSION}/${zdir}.tar.xz" && \
    tar -xf "${zdir}.tar.xz" -C /opt && \
    ln -s "/opt/${zdir}/zig" /usr/local/bin/zig && \
    rm "${zdir}.tar.xz"

# Prebuild raylib-nx so every build doesn't re-clone
RUN export DEVKITPRO=/opt/devkitpro && \
    export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH && \
    dkp-pacman -Sy --noconfirm switch-mesa switch-libdrm_nouveau && \
    cd /tmp && git clone --depth 1 https://github.com/Kapilarny/raylib-nx.git && \
    cd raylib-nx/src && \
    sed -i 's|^#define NX_SUPPORT_GAMEPAD_EMULATION|#//#define NX_SUPPORT_GAMEPAD_EMULATION|' config.h && \
    sed -i '1i #include <pthread.h>' switch-nx/dlfcn.h && \
    sed -i 's/^int pthread_attr_setschedpolicy/static inline int pthread_attr_setschedpolicy/' switch-nx/dlfcn.h && \
    sed -i 's/^int sched_get_priority_max/static inline int sched_get_priority_max/' switch-nx/dlfcn.h && \
    sed -i 's/^int sched_get_priority_min/static inline int sched_get_priority_min/' switch-nx/dlfcn.h && \
    sed -i '/MA_ENABLE_CUSTOM/a\    #include "switch-nx/dlfcn.h"' raudio.c && \
    make PLATFORM=PLATFORM_NX RAYLIB_LIBTYPE=STATIC -j"$(nproc)" && \
    cp libraylib.a $DEVKITPRO/portlibs/switch/lib/ && \
    cp raylib.h raymath.h rlgl.h $DEVKITPRO/portlibs/switch/include/ && \
    cd / && rm -rf /tmp/raylib-nx

ENV DEVKITPRO=/opt/devkitpro \
    PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:/usr/local/bin:/usr/bin:/bin

WORKDIR /work
