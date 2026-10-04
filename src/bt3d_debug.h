#ifndef BT3D_DEBUG_H
#define BT3D_DEBUG_H

typedef struct AppState AppState;

void bt3d_debug_log(const char *fmt, ...);
int bt3d_copy_debug_target_text(AppState *app);
void bt3d_draw_debug_overlay(AppState *app);

#endif
