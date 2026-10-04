#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_doors.h"
#include "bt3d_math.h"
#include "bt3d_world_classify.h"
#include "bt3d_world_collision.h"
#include "bt3d_world_doors.h"
#include "bt3d_world_grid.h"
#include "bt3d_world_markers.h"
#include "bt3d_world_textures.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    TEXTURE_FALLBACK_WALL = 1,
    TEXTURE_FALLBACK_FLOOR = 5,
    TEXTURE_FALLBACK_DOOR_NEIGHBOR = 30,
    TILE_LOW_BYTE_ALT_TEXTURE_A = 0x40,
    TILE_LOW_BYTE_ALT_TEXTURE_B = 0xc0,
    TILE_LOW_BYTE_EXIT = 0x6f,
    TILE_LOW_BYTE_EXIT_USED = 0x70,
    TILE_OVERLAY_FACE_MASK = 0x0300,
    TILE_OVERLAY_FACE_SHIFT = 8,
    /* Wall-like cells: classes 1-4 of the original cell classifier. */
    CELL_HAS_HIGH_DATA_MASK = 0x2000,
    /* Floor-class cells: classes 6-7 when the high-data bit is clear. */
    CELL_FLOOR_BIT = 0x1000
};

uint16_t bt3d_tile_at(const Map242 *map, int x, int y) {
    if (!map || !bt3d_cell_in_bounds(x, y)) {
        return TEXTURE_FALLBACK_WALL;
    }
    return map->tile_grid[bt3d_tile_index_for(x, y)];
}

float door_openness_at(const AppState *app, int x, int y) {
    DoorEvent *event = bt3d_find_event_by_cell(app, x, y);
    return event ? event->openness : 0.0f;
}

static int tile_uses_alt_texture(uint16_t tile) {
    int low_byte = tile & 0xff;
    return low_byte == TILE_LOW_BYTE_ALT_TEXTURE_A || low_byte == TILE_LOW_BYTE_ALT_TEXTURE_B;
}

static int first_valid_texture_code(const AppState *app, const int *codes, int count, int fallback) {
    int i = 0;
    for (i = 0; i < count; ++i) {
        if (bt3d_wall_texture(app, codes[i])) return codes[i];
    }
    return fallback;
}

int wall_texture_index_for_face(const AppState *app, int cell_x, int cell_y, int face_index) {
    int index;
    int code;

    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return TEXTURE_FALLBACK_WALL;
    index = bt3d_tile_index_for(cell_x, cell_y);
    if (tile_uses_alt_texture(app->content.map.tile_grid[index])) face_index = 0;
    code = app->content.map.detail_grid[index * 4 + face_index];
    return first_valid_texture_code(app, &code, 1, TEXTURE_FALLBACK_WALL);
}

static int raw_floor_texture_index_for_cell(const AppState *app, int cell_x, int cell_y) {
    const uint8_t *detail;
    int codes[4];
    int index;

    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return TEXTURE_FALLBACK_FLOOR;
    index = bt3d_tile_index_for(cell_x, cell_y);
    detail = &app->content.map.detail_grid[index * 4];
    codes[0] = detail[3];
    codes[1] = detail[2];
    if (tile_uses_alt_texture(app->content.map.tile_grid[index])) {
        codes[2] = detail[0];
        codes[3] = detail[1];
        return first_valid_texture_code(app, codes, 4, TEXTURE_FALLBACK_DOOR_NEIGHBOR);
    }
    codes[2] = detail[1];
    codes[3] = detail[0];
    return first_valid_texture_code(app, codes, 4, TEXTURE_FALLBACK_FLOOR);
}

static int raw_ceiling_texture_index_for_cell(const AppState *app, int cell_x, int cell_y) {
    const uint8_t *detail;
    int codes[4];
    int index;

    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return TEXTURE_FALLBACK_WALL;
    index = bt3d_tile_index_for(cell_x, cell_y);
    detail = &app->content.map.detail_grid[index * 4];
    codes[0] = detail[0];
    codes[1] = detail[2];
    if (tile_uses_alt_texture(app->content.map.tile_grid[index])) {
        codes[2] = detail[3];
        return first_valid_texture_code(app, codes, 3, TEXTURE_FALLBACK_DOOR_NEIGHBOR);
    }
    codes[2] = detail[1];
    codes[3] = detail[3];
    return first_valid_texture_code(app, codes, 4, TEXTURE_FALLBACK_WALL);
}

static int door_neighbor_texture_index(const AppState *app, int cell_x, int cell_y, int (*raw_index_fn)(const AppState *, int, int), int fallback) {
    static const int offsets[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    int i;

    if (!bt3d_find_event_by_cell(app, cell_x, cell_y)) {
        return raw_index_fn(app, cell_x, cell_y);
    }
    for (i = 0; i < 4; ++i) {
        int next_x = cell_x + offsets[i][0];
        int next_y = cell_y + offsets[i][1];
        if (!bt3d_cell_in_bounds(next_x, next_y)) continue;
        if (bt3d_find_event_by_cell(app, next_x, next_y)) continue;
        if (map242_is_solid(bt3d_tile_at(&app->content.map, next_x, next_y))) continue;
        return raw_index_fn(app, next_x, next_y);
    }
    return fallback;
}

int floor_texture_index_for_cell(const AppState *app, int cell_x, int cell_y) {
    return door_neighbor_texture_index(app, cell_x, cell_y, raw_floor_texture_index_for_cell, TEXTURE_FALLBACK_FLOOR);
}

int ceiling_texture_index_for_cell(const AppState *app, int cell_x, int cell_y) {
    return door_neighbor_texture_index(app, cell_x, cell_y, raw_ceiling_texture_index_for_cell, TEXTURE_FALLBACK_WALL);
}

void bt3d_build_static_cell_caches(AppState *app) {
    int x, y;
    for (y = 0; y < BT3D_MAP_HEIGHT; ++y) {
        for (x = 0; x < BT3D_MAP_WIDTH; ++x) {
            int idx = bt3d_tile_index_for(x, y);
            int raw_floor = raw_floor_texture_index_for_cell(app, x, y);
            int raw_ceil = raw_ceiling_texture_index_for_cell(app, x, y);
            int structural = is_structural_wall_tile(app, x, y) ? 1 : 0;
            app->map_state.raw_floor_tex_cache[idx] = (unsigned char)(raw_floor & 0xff);
            app->map_state.raw_ceil_tex_cache[idx] = (unsigned char)(raw_ceil & 0xff);
            app->map_state.structural_wall_mask[idx] = (unsigned char)structural;
        }
    }
}

int door_overlay_index_for_cell(const AppState *app, int cell_x, int cell_y) {
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;
    return app->content.map.detail_grid[bt3d_tile_index_for(cell_x, cell_y) * 4 + 1];
}

int wall_overlay_index_for_face(const AppState *app, int cell_x, int cell_y, int face_index) {
    int index;
    uint16_t tile;
    int low_byte;

    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;
    index = bt3d_tile_index_for(cell_x, cell_y);
    tile = app->content.map.tile_grid[index];
    if (tile_uses_alt_texture(tile)) {
        return app->content.map.detail_grid[index * 4 + 1];
    }
    low_byte = tile & 0xff;
    if (low_byte > 0 && ((tile & TILE_OVERLAY_FACE_MASK) >> TILE_OVERLAY_FACE_SHIFT) == face_index) {
        return low_byte;
    }
    return 0;
}

int is_structural_wall_tile(const AppState *app, int cell_x, int cell_y) {
    uint16_t tile = app->content.map.tile_grid[bt3d_tile_index_for(cell_x, cell_y)];
    if (map242_is_door(tile) || map242_is_special_prop_tile(tile)) return 0;
    if (cell_has_ceiling_marker(app, cell_x, cell_y)) return 0;
    return (tile & CELL_HAS_HIGH_DATA_MASK) != 0;
}

/* Turns an unused exit switch into a used one; returns whether there was one. */
int bt3d_consume_exit_tile(AppState *app, int cell_x, int cell_y) {
    uint16_t *tile;
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;
    tile = &app->content.map.tile_grid[bt3d_tile_index_for(cell_x, cell_y)];
    if ((*tile & 0xff) != TILE_LOW_BYTE_EXIT) return 0;
    *tile = (uint16_t)((*tile & 0xff00) | TILE_LOW_BYTE_EXIT_USED);
    return 1;
}

int bt3d_is_remote_trigger_tile(uint16_t tile) {
    return (tile & (CELL_HAS_HIGH_DATA_MASK | CELL_FLOOR_BIT)) == CELL_FLOOR_BIT;
}

int is_structural_wall_for_door_axis(const AppState *app, int cell_x, int cell_y) {
    uint16_t tile = 0;
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) {
        return 1;
    }
    if (bt3d_find_event_by_cell(app, cell_x, cell_y)) {
        return 0;
    }
    if (cell_has_ceiling_marker(app, cell_x, cell_y)) {
        return 0;
    }
    tile = bt3d_tile_at(&app->content.map, cell_x, cell_y);
    if (map242_is_special_prop_tile(tile)) {
        return 0;
    }
    return (tile & BT3D_TILE_SOLID_BIT) != 0;
}

/* A door slides along x when walls flank it on both the west and the east. */
int door_slides_on_x_axis(const AppState *app, int cell_x, int cell_y) {
    return is_structural_wall_for_door_axis(app, cell_x - 1, cell_y)
        && is_structural_wall_for_door_axis(app, cell_x + 1, cell_y);
}

int bt3d_is_blocked_cell(const AppState *app, int x, int y) {
    DoorEvent *event = NULL;
    uint16_t tile;
    if (!bt3d_cell_in_bounds(x, y)) {
        return 1;
    }
    event = bt3d_find_event_by_cell(app, x, y);
    if (event) {
        return event->openness <= 0.0f;
    }
    if (cell_has_ceiling_marker(app, x, y)) {
        return 0;
    }
    tile = bt3d_tile_at(&app->content.map, x, y);
    return map242_is_solid(tile);
}

int bt3d_is_walkable(const AppState *app, Vector3 position) {
    int min_x = (int)floorf(position.x - PLAYER_RADIUS);
    int max_x = (int)floorf(position.x + PLAYER_RADIUS);
    int min_z = (int)floorf(position.z - PLAYER_RADIUS);
    int max_z = (int)floorf(position.z + PLAYER_RADIUS);
    int x = 0;
    int z = 0;
    for (z = min_z; z <= max_z; ++z) {
        for (x = min_x; x <= max_x; ++x) {
            if (bt3d_is_blocked_cell(app, x, z)) {
                return 0;
            }
        }
    }
    return 1;
}
