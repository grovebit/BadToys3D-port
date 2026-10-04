#ifndef BT3D_RUNTIME_H
#define BT3D_RUNTIME_H

#include "bt3d_app_state.h"

typedef struct {
    AppState app;
    Camera3D camera;
} Bt3dRuntime;

/* Startup, frames and shutdown, shared by every platform's entry point.
   Init returns 0 when no window could be opened. */
int bt3d_runtime_init(Bt3dRuntime *runtime);
int bt3d_runtime_should_close(const Bt3dRuntime *runtime);
void bt3d_runtime_frame(Bt3dRuntime *runtime);
void bt3d_runtime_shutdown(Bt3dRuntime *runtime);

#endif
