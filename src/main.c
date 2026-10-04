#include "bt3d_runtime.h"

#if BT3D_PLATFORM_SWITCH
#include <switch.h>
#endif

#if BT3D_PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

static void run_frame(void *runtime) {
    bt3d_runtime_frame((Bt3dRuntime *)runtime);
}

/* raylib's Android glue calls main with arguments. */
int main(int argc, char **argv) {
    static Bt3dRuntime runtime;
    (void)argc;
    (void)argv;

#if BT3D_PLATFORM_SWITCH
    socketInitializeDefault();
    nxlinkStdio();
    romfsInit();
#endif

    if (!bt3d_runtime_init(&runtime)) return 1;

#if BT3D_PLATFORM_WEB
    /* Paced by requestAnimationFrame; the page owns the lifetime, so this
       never returns and the game has no Quit item on the web. */
    emscripten_set_main_loop_arg(run_frame, &runtime, 0, 1);
#else
    while (!bt3d_runtime_should_close(&runtime)) {
        run_frame(&runtime);
    }
    bt3d_runtime_shutdown(&runtime);
#endif

#if BT3D_PLATFORM_SWITCH
    romfsExit();
    socketExit();
#endif

    return 0;
}
