#ifndef BT3D_WORLD_GRID_H
#define BT3D_WORLD_GRID_H

#include "map242.h"

/* bt3d_cell_in_bounds and bt3d_tile_index_for are inline in map242.h. */
uint16_t bt3d_tile_at(const Map242 *map, int x, int y);

#endif
