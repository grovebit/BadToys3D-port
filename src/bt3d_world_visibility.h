#ifndef BT3D_WORLD_VISIBILITY_H
#define BT3D_WORLD_VISIBILITY_H

#include "raylib.h"

#include <math.h>

typedef struct AppState AppState;

/* Grid DDA walk: each step enters the next cell the ray crosses. */
typedef struct {
    int map_x;
    int map_y;
    int step_x;
    int step_y;
    float delta_dist_x;
    float delta_dist_y;
    float side_dist_x;
    float side_dist_y;
} Bt3dGridRay;

static inline Bt3dGridRay bt3d_grid_ray_begin(Vector2 origin, Vector2 direction) {
    Bt3dGridRay ray;
    ray.map_x = (int)floorf(origin.x);
    ray.map_y = (int)floorf(origin.y);
    ray.delta_dist_x = fabsf(1.0f / (fabsf(direction.x) > 1.0e-6f ? direction.x : 1.0e-6f));
    ray.delta_dist_y = fabsf(1.0f / (fabsf(direction.y) > 1.0e-6f ? direction.y : 1.0e-6f));
    ray.step_x = direction.x < 0.0f ? -1 : 1;
    ray.step_y = direction.y < 0.0f ? -1 : 1;
    ray.side_dist_x = direction.x < 0.0f
        ? (origin.x - (float)ray.map_x) * ray.delta_dist_x
        : ((float)(ray.map_x + 1) - origin.x) * ray.delta_dist_x;
    ray.side_dist_y = direction.y < 0.0f
        ? (origin.y - (float)ray.map_y) * ray.delta_dist_y
        : ((float)(ray.map_y + 1) - origin.y) * ray.delta_dist_y;
    return ray;
}

/* Moves into the next cell and returns the ray distance at which it enters. */
static inline float bt3d_grid_ray_step(Bt3dGridRay *ray) {
    float entry_distance;
    if (ray->side_dist_x < ray->side_dist_y) {
        entry_distance = ray->side_dist_x;
        ray->side_dist_x += ray->delta_dist_x;
        ray->map_x += ray->step_x;
    } else {
        entry_distance = ray->side_dist_y;
        ray->side_dist_y += ray->delta_dist_y;
        ray->map_y += ray->step_y;
    }
    return entry_distance;
}

int bt3d_is_visibility_blocked_cell(const AppState *app, int x, int y);
int bt3d_has_line_of_sight(const AppState *app, Vector2 from_pos, Vector2 to_pos);
int bt3d_is_tile_visible_to_player(const AppState *app, int cell_x, int cell_y);
void bt3d_refresh_visible_tile_mask(AppState *app);

#endif
