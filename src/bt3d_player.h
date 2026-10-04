#ifndef BT3D_PLAYER_H
#define BT3D_PLAYER_H

#include "raylib.h"
#include "bt3d_input.h"

typedef struct AppState AppState;

int bt3d_player_weapon_selectable(const AppState *app, int slot);
void bt3d_select_previous_available_weapon(AppState *app);
void bt3d_update_player(AppState *app, const FrameInput *input, Camera3D *camera);
void bt3d_update_camera_target(Camera3D *camera, const AppState *app);
Vector3 bt3d_player_forward(const AppState *app);
Vector2 bt3d_player_forward_xz(const AppState *app);

#endif
