#include "bt3d_app_state.h"
#include "bt3d_world_collision.h"
#include "player_internal.h"

#include "raymath.h"

static int player_enemy_collision_blocks_move(AppState *app, Vector3 from, Vector3 to) {
    const float min_distance = PLAYER_RADIUS + ENEMY_RADIUS;
    const float min_distance_sq = min_distance * min_distance;
    int i;

    for (i = 0; i < app->entities.enemy_count; ++i) {
        EnemyRuntime *enemy = &app->entities.enemies[i];
        Vector2 enemy_pos;
        Vector2 from_delta;
        Vector2 to_delta;
        float from_dist_sq;
        float to_dist_sq;

        if (!enemy->alive) continue;

        enemy_pos = (Vector2){ enemy->position.x, enemy->position.z };
        from_delta = Vector2Subtract((Vector2){ from.x, from.z }, enemy_pos);
        to_delta = Vector2Subtract((Vector2){ to.x, to.z }, enemy_pos);
        from_dist_sq = Vector2LengthSqr(from_delta);
        to_dist_sq = Vector2LengthSqr(to_delta);

        if (to_dist_sq < min_distance_sq && to_dist_sq <= from_dist_sq) {
            return 1;
        }
    }

    return 0;
}

void bt3d_player_apply_movement(AppState *app, Vector3 move, float dt) {
    if (Vector3Length(move) <= 0.0f) return;
    move = Vector3Scale(Vector3Normalize(move), MOVE_SPEED * dt);

    if (app->control.noclip_enabled) {
        app->player.position.x += move.x;
        app->player.position.z += move.z;
    } else {
        Vector3 next = app->player.position;
        next.x += move.x;
        if (bt3d_is_walkable(app, next) && !player_enemy_collision_blocks_move(app, app->player.position, next)) {
            app->player.position.x = next.x;
        }

        next = app->player.position;
        next.z += move.z;
        if (bt3d_is_walkable(app, next) && !player_enemy_collision_blocks_move(app, app->player.position, next)) {
            app->player.position.z = next.z;
        }
    }
}
