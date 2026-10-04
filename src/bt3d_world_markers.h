#ifndef BT3D_WORLD_MARKERS_H
#define BT3D_WORLD_MARKERS_H

#include "raylib.h"
#include "bt3d_types.h"

typedef struct AppState AppState;

Vector2 marker_world_pos(const MarkerRuntime *marker);
int marker_hangs_from_ceiling(const AppState *app, int marker_type);
int cell_has_ceiling_marker(const AppState *app, int cell_x, int cell_y);

#endif
