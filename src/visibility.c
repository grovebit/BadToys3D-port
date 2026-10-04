#include "bt3d_app_state.h"
#include "bt3d_doors.h"
#include "bt3d_world_visibility.h"

#include "raymath.h"

#include <math.h>
#include <string.h>

int bt3d_is_visibility_blocked_cell(const AppState *app, int x, int y) {
    const DoorEvent *event;

    if (!bt3d_cell_in_bounds(x, y)) return 1;
    event = bt3d_find_event_by_cell(app, x, y);
    if (event) return event->openness <= 0.0f;
    /* Built from is_structural_wall_tile at map load; nothing that changes at
       runtime affects it. */
    return app->map_state.structural_wall_mask[bt3d_tile_index_for(x, y)];
}

/* Blocked when any cell the segment passes through is. */
int bt3d_has_line_of_sight(const AppState *app, Vector2 from_pos, Vector2 to_pos) {
    Vector2 delta = Vector2Subtract(to_pos, from_pos);
    float distance = Vector2Length(delta);
    Bt3dGridRay ray;

    if (distance < 0.001f) return 1;
    ray = bt3d_grid_ray_begin(from_pos, Vector2Scale(delta, 1.0f / distance));
    while (bt3d_grid_ray_step(&ray) < distance) {
        if (bt3d_is_visibility_blocked_cell(app, ray.map_x, ray.map_y)) return 0;
    }
    return 1;
}

int bt3d_is_tile_visible_to_player(const AppState *app, int cell_x, int cell_y) {
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;
    return app->map_state.visible_tile_mask[bt3d_tile_index_for(cell_x, cell_y)] != 0;
}

static void mark_visible(MapRuntimeState *map_state, int x, int y, uint16_t *visible, int *visible_count) {
    int index;
    if (!bt3d_cell_in_bounds(x, y)) return;
    index = bt3d_tile_index_for(x, y);
    if (map_state->visible_tile_mask[index]) return;
    map_state->visible_tile_mask[index] = 1;
    visible[(*visible_count)++] = (uint16_t)index;
}

static void add_render_cell(MapRuntimeState *map_state, int index) {
    if (map_state->render_tile_mask[index]) return;
    map_state->render_tile_mask[index] = 1;
    map_state->render_cell_indices[map_state->render_cell_count++] = (uint16_t)index;
}

void bt3d_refresh_visible_tile_mask(AppState *app) {
    static const int neighbours[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    static Vector2 ray_directions[VISIBILITY_MASK_RAY_COUNT];
    static int ray_directions_ready = 0;
    MapRuntimeState *map_state = &app->map_state;
    Vector2 origin = { app->player.position.x, app->player.position.z };
    int player_x = (int)floorf(origin.x);
    int player_y = (int)floorf(origin.y);
    uint16_t visible[BT3D_GRID_CELLS];
    int visible_count = 0;
    int i;
    int n;

    /* Visibility is gathered all around the player, so turning changes nothing. */
    if (map_state->visibility_valid && map_state->visibility_origin.x == origin.x && map_state->visibility_origin.y == origin.y
        && map_state->visibility_doors_version == map_state->doors_version) return;
    map_state->visibility_valid = 1;
    map_state->visibility_origin = origin;
    map_state->visibility_doors_version = map_state->doors_version;

    if (!ray_directions_ready) {
        for (i = 0; i < VISIBILITY_MASK_RAY_COUNT; ++i) {
            float angle = ((float)i / (float)VISIBILITY_MASK_RAY_COUNT) * PI * 2.0f;
            ray_directions[i] = (Vector2){ cosf(angle), sinf(angle) };
        }
        ray_directions_ready = 1;
    }

    memset(map_state->visible_tile_mask, 0, sizeof(map_state->visible_tile_mask));
    memset(map_state->render_tile_mask, 0, sizeof(map_state->render_tile_mask));
    map_state->render_cell_count = 0;

    /* The 3x3 block around the player, then a ray fan stopped by walls and closed doors. */
    for (i = 0; i < 9; ++i) {
        mark_visible(map_state, player_x + i % 3 - 1, player_y + i / 3 - 1, visible, &visible_count);
    }
    for (i = 0; i < VISIBILITY_MASK_RAY_COUNT; ++i) {
        Bt3dGridRay ray = bt3d_grid_ray_begin(origin, ray_directions[i]);
        int step;
        for (step = 0; step < MAX_RAY_STEPS; ++step) {
            bt3d_grid_ray_step(&ray);
            if (!bt3d_cell_in_bounds(ray.map_x, ray.map_y)) break;
            mark_visible(map_state, ray.map_x, ray.map_y, visible, &visible_count);
            if (bt3d_is_visibility_blocked_cell(app, ray.map_x, ray.map_y)) break;
        }
    }

    /* Visible cells are drawn with their neighbours and appear on the
       automap, as do the walls and doors next to them. */
    for (i = 0; i < visible_count; ++i) {
        int x = visible[i] / BT3D_MAP_WIDTH;
        int y = visible[i] % BT3D_MAP_WIDTH;

        map_state->discovered_tile_mask[visible[i]] = 1;
        add_render_cell(map_state, visible[i]);
        for (n = 0; n < 4; ++n) {
            int nx = x + neighbours[n][0];
            int ny = y + neighbours[n][1];
            int neighbour;
            if (!bt3d_cell_in_bounds(nx, ny)) continue;
            neighbour = bt3d_tile_index_for(nx, ny);
            add_render_cell(map_state, neighbour);
            if (map_state->event_index_by_cell[neighbour] >= 0 || bt3d_is_visibility_blocked_cell(app, nx, ny)) {
                map_state->discovered_tile_mask[neighbour] = 1;
            }
        }
    }
}
