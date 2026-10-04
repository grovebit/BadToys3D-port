#ifndef BT3D_SAVE_INTERNAL_H
#define BT3D_SAVE_INTERNAL_H

#include "bt3d_app_state.h"

#include <stdint.h>

/* A save file is this header followed by the counted doors, markers,
   enemies, player projectiles and enemy projectiles, stored as raw structs. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    int map_index;
    int difficulty;
    PlayerState player;
    int event_count;
    int marker_count;
    int enemy_count;
    int player_projectile_count;
    int enemy_projectile_count;
    unsigned char discovered_tile_mask[BT3D_GRID_CELLS];
} SaveHeader;

/* Reads the header of a save this build can load. */
int bt3d_read_save_header(const char *path, SaveHeader *header);

#endif
