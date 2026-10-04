#ifndef BT3D_MAP242_H
#define BT3D_MAP242_H

#include <stddef.h>
#include <stdint.h>

#define BT3D_MAP_WIDTH 64
#define BT3D_MAP_HEIGHT 64
#define BT3D_GRID_CELLS (BT3D_MAP_WIDTH * BT3D_MAP_HEIGHT)

/* High bit of a tile word: set on walls and on every door family. */
#define BT3D_TILE_SOLID_BIT 0x8000u

typedef struct {
    uint8_t cell_x;
    uint8_t cell_y;
    uint16_t type;
} MapMarker;

typedef struct {
    int32_t pos_x;
    int32_t pos_y;
    uint16_t heading_degrees;
    uint8_t class_id;
    uint8_t frame;
    uint8_t state;
} MapObject;

typedef struct {
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t state;
} MapEvent;

typedef struct {
    float tile_x;
    float tile_y;
    float angle_radians;
} MapSpawn;

typedef struct {
    char name[9];
    uint16_t tile_grid[BT3D_GRID_CELLS];
    uint8_t detail_grid[BT3D_GRID_CELLS * 4];
    /* 255 (kind, cell_x, cell_y) triplets, addressed by a trigger tile's
       low byte minus one. */
    uint8_t descriptors[255 * 3];
    MapMarker *markers;
    size_t marker_count;
    MapObject *objects;
    size_t object_count;
    MapEvent *events;
    size_t event_count;
    MapSpawn spawn;
} Map242;

static inline int bt3d_cell_in_bounds(int x, int y) {
    return x >= 0 && y >= 0 && x < BT3D_MAP_WIDTH && y < BT3D_MAP_HEIGHT;
}

/* Cell grids are stored column-major. */
static inline int bt3d_tile_index_for(int x, int y) {
    return x * BT3D_MAP_WIDTH + y;
}

int map242_parse(Map242 *map, const char *entry_name, const unsigned char *bytes, size_t length);
void map242_unload(Map242 *map);
int map242_is_solid(uint16_t tile);
int map242_is_door(uint16_t tile);
int map242_is_special_prop_tile(uint16_t tile);
int map242_is_player_start_object(const MapObject *object);

#endif
