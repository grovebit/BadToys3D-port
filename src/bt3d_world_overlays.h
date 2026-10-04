#ifndef BT3D_WORLD_OVERLAYS_H
#define BT3D_WORLD_OVERLAYS_H

typedef struct AppState AppState;

/* The state lives in the map state, so animation restarts with each map. */
void bt3d_update_overlay_animations(AppState *app, float dt);
int bt3d_animated_overlay_id(const AppState *app, int base_id);

#endif
