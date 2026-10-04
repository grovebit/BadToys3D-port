#ifndef BT3D_WORLD_COLLISION_H
#define BT3D_WORLD_COLLISION_H

#include "raylib.h"

typedef struct AppState AppState;

int bt3d_is_blocked_cell(const AppState *app, int x, int y);
int bt3d_is_walkable(const AppState *app, Vector3 position);

#endif
