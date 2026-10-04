#ifndef BT3D_WORLD_CLASSIFY_H
#define BT3D_WORLD_CLASSIFY_H

#include <stdint.h>

typedef struct AppState AppState;

int is_structural_wall_tile(const AppState *app, int cell_x, int cell_y);
int is_structural_wall_for_door_axis(const AppState *app, int cell_x, int cell_y);
int bt3d_is_remote_trigger_tile(uint16_t tile);
/* Marks an exit switch as used; returns whether the cell held one. */
int bt3d_consume_exit_tile(AppState *app, int cell_x, int cell_y);
void bt3d_build_static_cell_caches(AppState *app);

#endif
