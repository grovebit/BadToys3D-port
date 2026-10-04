#include "bt3d_app_state.h"
#include "bt3d_constants.h"
#include "bt3d_world_collision.h"
#include "bt3d_world_spawn.h"

#include <string.h>

void init_player_state(AppState *app) {
    static const PlayerState fresh = {
        .health = 99,
        .lives = 3,
        .weapon_slots = { 0, 1, 1, 0, 0 },
        .weapon_ammo = { 0, 99, 24, 0, 10 },
        .current_weapon_slot = 2,
    };
    app->player = fresh;
}

/* Stands the player on the map's start, nudged clear of any wall it touches. */
void place_player_at_spawn(AppState *app) {
    static const Vector2 offsets[] = {
        { 0.0f, 0.0f }, { 0.25f, 0.0f }, { -0.25f, 0.0f }, { 0.0f, 0.25f }, { 0.0f, -0.25f },
        { 0.35f, 0.35f }, { -0.35f, 0.35f }, { 0.35f, -0.35f }, { -0.35f, -0.35f },
        { 0.5f, 0.0f }, { -0.5f, 0.0f }, { 0.0f, 0.5f }, { 0.0f, -0.5f }
    };
    Vector3 spawn = { app->content.map.spawn.tile_x, PLAYER_HEIGHT, app->content.map.spawn.tile_y };
    int i;

    app->player.yaw = (PI * 0.5f) - app->content.map.spawn.angle_radians;
    app->player.position = spawn;
    for (i = 0; i < (int)(sizeof(offsets) / sizeof(offsets[0])); ++i) {
        Vector3 candidate = { spawn.x + offsets[i].x, spawn.y, spawn.z + offsets[i].y };
        if (bt3d_is_walkable(app, candidate)) {
            app->player.position = candidate;
            return;
        }
    }
}
