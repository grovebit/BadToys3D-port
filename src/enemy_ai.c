#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_combat.h"
#include "bt3d_doors.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_world_collision.h"
#include "bt3d_world_visibility.h"

#include "raymath.h"

#include <math.h>
#include <stdint.h>

enum {
    ENEMY_CHASE_FIELD_MAX_CELLS = 24
};

static const float ENEMY_DOOR_INTERACTION_DISTANCE = 1.1f;
static const float ENEMY_DOOR_MIN_DISTANCE = 1.0e-4f;
static const float ENEMY_DOOR_FACING_DOT_MIN = 0.45f;
static const float ENEMY_OTHER_COLLISION_RADIUS = 0.18f;
static const float ENEMY_AI_ACTIVE_DISTANCE = 22.0f;
static const float ENEMY_MIN_PLAYER_DISTANCE = 0.0001f;
static const float ENEMY_MOVED_EPSILON = 1.0e-4f;
static const float ENEMY_HURT_ATTACK_COOLDOWN_SCALE = 0.2f;
static const float ENEMY_ATTACK_ANIMATION_TIME = 0.24f;
static const float ENEMY_AGGRO_SOUND_VOLUME = 0.65f;
static const float ENEMY_ATTACK_SOUND_VOLUME = 0.75f;

static int try_open_door_for_enemy(AppState *app, EnemyRuntime *enemy, Vector2 target_pos) {
    Vector2 to_target;
    float distance_to_target;
    Vector2 forward;
    int i = 0;

    to_target = Vector2Subtract(target_pos, (Vector2){ enemy->position.x, enemy->position.z });
    distance_to_target = Vector2Length(to_target);
    if (distance_to_target <= 1.0e-5f) return 0;
    forward = Vector2Scale(to_target, 1.0f / distance_to_target);

    for (i = 0; i < app->entities.event_count; ++i) {
        DoorEvent *event = &app->entities.events[i];
        Vector2 delta;
        float distance;
        float facing;

        if (bt3d_required_key_slot_for_event_type(event->base.type) > 0) continue;
        if (event->openness >= BT3D_DOOR_OPEN_THRESHOLD) continue;

        delta = Vector2Subtract(bt3d_door_center(event), (Vector2){ enemy->position.x, enemy->position.z });
        if (Vector2LengthSqr(delta) > ENEMY_DOOR_INTERACTION_DISTANCE * ENEMY_DOOR_INTERACTION_DISTANCE) continue;
        distance = Vector2Length(delta);
        if (distance < ENEMY_DOOR_MIN_DISTANCE) continue;

        facing = Vector2DotProduct(Vector2Scale(delta, 1.0f / distance), forward);
        if (facing < ENEMY_DOOR_FACING_DOT_MIN) continue;

        bt3d_request_door_hold_open(app, event);
        return 1;
    }

    return 0;
}

static int can_enemy_occupy_static(const AppState *app, float x, float z, float radius) {
    float sample_radius = bt3d_clamp_f(radius * 0.55f, 0.08f, 0.16f);
    const float samples[4][2] = {
        { -sample_radius, -sample_radius },
        { sample_radius, -sample_radius },
        { -sample_radius, sample_radius },
        { sample_radius, sample_radius }
    };
    int i = 0;

    for (i = 0; i < 4; ++i) {
        int cell_x = (int)floorf(x + samples[i][0]);
        int cell_y = (int)floorf(z + samples[i][1]);
        if (bt3d_is_blocked_cell(app, cell_x, cell_y)) {
            return 0;
        }
    }

    return 1;
}

static int can_enemy_occupy(const AppState *app, float x, float z, float radius, int self_index) {
    float min_distance = radius + ENEMY_OTHER_COLLISION_RADIUS;
    int i;

    if (!can_enemy_occupy_static(app, x, z, radius)) return 0;
    for (i = 0; i < app->entities.enemy_count; ++i) {
        const EnemyRuntime *other = &app->entities.enemies[i];
        if (i == self_index || !other->alive) continue;
        if (Vector2DistanceSqr((Vector2){ x, z }, (Vector2){ other->position.x, other->position.z }) < min_distance * min_distance) {
            return 0;
        }
    }
    return 1;
}

/* Cells are tested whole: an occupancy footprint centred in a cell never
   reaches past it, so only the cell's own blocked state matters. */
static void build_chase_distance_field(const AppState *app, int target_cell_x, int target_cell_y, int16_t max_distance, int16_t *distances) {
    int16_t queue_x[BT3D_GRID_CELLS];
    int16_t queue_y[BT3D_GRID_CELLS];
    int head = 0;
    int tail = 0;
    int i = 0;
    const int offsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    for (i = 0; i < BT3D_GRID_CELLS; ++i) {
        distances[i] = -1;
    }
    if (!bt3d_cell_in_bounds(target_cell_x, target_cell_y) || bt3d_is_blocked_cell(app, target_cell_x, target_cell_y)) {
        return;
    }

    distances[bt3d_tile_index_for(target_cell_x, target_cell_y)] = 0;
    queue_x[tail] = (int16_t)target_cell_x;
    queue_y[tail] = (int16_t)target_cell_y;
    tail++;

    while (head < tail) {
        int cell_x = queue_x[head];
        int cell_y = queue_y[head];
        int16_t base_distance = distances[bt3d_tile_index_for(cell_x, cell_y)];
        head++;

        if (base_distance >= max_distance) {
            continue;
        }

        for (i = 0; i < 4; ++i) {
            int next_x = cell_x + offsets[i][0];
            int next_y = cell_y + offsets[i][1];
            int index;
            if (!bt3d_cell_in_bounds(next_x, next_y)) continue;
            index = bt3d_tile_index_for(next_x, next_y);
            if (distances[index] != -1) continue;
            if (bt3d_is_blocked_cell(app, next_x, next_y)) continue;
            distances[index] = base_distance + 1;
            queue_x[tail] = (int16_t)next_x;
            queue_y[tail] = (int16_t)next_y;
            tail++;
        }
    }
}

static int choose_path_step_from_distance_field(const AppState *app, EnemyRuntime *enemy, const int16_t *distances, float step, Vector2 *out_step) {
    const int offsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int cell_x;
    int cell_y;
    int16_t current_distance;
    int16_t best_distance = INT16_MAX;
    Vector2 best = { 0 };
    int found = 0;
    int i = 0;
    float radius = ENEMY_RADIUS;

    cell_x = (int)floorf(enemy->position.x);
    cell_y = (int)floorf(enemy->position.z);
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;

    current_distance = distances[bt3d_tile_index_for(cell_x, cell_y)];
    if (current_distance <= 0) return 0;

    for (i = 0; i < 4; ++i) {
        int next_cell_x = cell_x + offsets[i][0];
        int next_cell_y = cell_y + offsets[i][1];
        int index;
        int16_t next_distance;
        float target_x;
        float target_z;
        float dir_x;
        float dir_z;
        float dir_length;
        Vector2 move;

        if (!bt3d_cell_in_bounds(next_cell_x, next_cell_y)) continue;
        index = bt3d_tile_index_for(next_cell_x, next_cell_y);
        next_distance = distances[index];
        if (next_distance < 0 || next_distance >= current_distance) continue;

        target_x = next_cell_x + 0.5f;
        target_z = next_cell_y + 0.5f;
        dir_x = target_x - enemy->position.x;
        dir_z = target_z - enemy->position.z;
        dir_length = hypotf(dir_x, dir_z);
        if (dir_length <= 1.0e-6f) continue;
        move = (Vector2){ (dir_x / dir_length) * step, (dir_z / dir_length) * step };
        if (!can_enemy_occupy_static(app, enemy->position.x + move.x, enemy->position.z + move.y, radius)) continue;

        if (!found || next_distance < best_distance) {
            best_distance = next_distance;
            best = move;
            found = 1;
        }
    }

    if (found) {
        *out_step = best;
    }
    return found;
}

static int choose_enemy_chase_step(AppState *app, EnemyRuntime *enemy, float step, Vector2 target, int self_index, Vector2 *out_step) {
    static const float offsets[] = {
        0.0f,
        PI / 10.0f, -PI / 10.0f,
        PI / 5.0f, -PI / 5.0f,
        PI / 3.0f, -PI / 3.0f,
        PI / 2.0f, -PI / 2.0f
    };
    float direct_angle;
    float best_score = 1.0e30f;
    Vector2 best = { 0 };
    int found = 0;
    int i = 0;
    float radius = ENEMY_RADIUS;

    direct_angle = atan2f(target.y - enemy->position.z, target.x - enemy->position.x);
    for (i = 0; i < ARRAY_COUNT(offsets); ++i) {
        float angle = direct_angle + offsets[i];
        float move_x = cosf(angle) * step;
        float move_z = sinf(angle) * step;
        float next_x = enemy->position.x + move_x;
        float next_z = enemy->position.z + move_z;
        float next_distance;
        float score;

        if (!can_enemy_occupy(app, next_x, next_z, radius, self_index)) continue;
        next_distance = hypotf(target.x - next_x, target.y - next_z);
        score = next_distance + fabsf(offsets[i]) * 0.35f;
        if (!found || score < best_score) {
            best_score = score;
            best = (Vector2){ move_x, move_z };
            found = 1;
        }
    }

    if (found) {
        *out_step = best;
    }
    return found;
}

static int move_enemy_with_collision(AppState *app, EnemyRuntime *enemy, Vector2 move, int self_index) {
    float start_x;
    float start_z;
    float radius = ENEMY_RADIUS;

    start_x = enemy->position.x;
    start_z = enemy->position.z;

    if (can_enemy_occupy(app, enemy->position.x + move.x, enemy->position.z, radius, self_index)) {
        enemy->position.x += move.x;
    }
    if (can_enemy_occupy(app, enemy->position.x, enemy->position.z + move.y, radius, self_index)) {
        enemy->position.z += move.y;
    }

    return fabsf(enemy->position.x - start_x) > ENEMY_MOVED_EPSILON || fabsf(enemy->position.z - start_z) > ENEMY_MOVED_EPSILON;
}

/* Distances to the player's cell, rebuilt only when the player changes cell
   or a door opens or closes. */
static const int16_t *chase_distance_field(AppState *app) {
    MapRuntimeState *map_state = &app->map_state;
    int cell_x = (int)floorf(app->player.position.x);
    int cell_y = (int)floorf(app->player.position.z);
    int target = bt3d_cell_in_bounds(cell_x, cell_y) ? bt3d_tile_index_for(cell_x, cell_y) : -2;

    if (target != map_state->chase_target_cell || map_state->chase_doors_version != map_state->doors_version) {
        map_state->chase_target_cell = target;
        map_state->chase_doors_version = map_state->doors_version;
        build_chase_distance_field(app, cell_x, cell_y, ENEMY_CHASE_FIELD_MAX_CELLS, map_state->chase_distances);
    }
    return map_state->chase_distances;
}

static void update_enemy_timers(EnemyRuntime *enemy, float dt, int far_update) {
    if (enemy->attack_cooldown > 0.0f) {
        float scale = (!far_update && enemy->hurt_timer > 0.0f) ? ENEMY_HURT_ATTACK_COOLDOWN_SCALE : 1.0f;
        enemy->attack_cooldown -= dt * scale;
    }
    if (enemy->attack_timer > 0.0f) enemy->attack_timer -= dt;
    if (enemy->hurt_timer > 0.0f) enemy->hurt_timer -= dt;
}

static void maybe_aggro_enemy(AppState *app, EnemyRuntime *enemy, float distance, int has_aggro_los) {

    if (!enemy->aggroed && distance >= ENEMY_MIN_PLAYER_DISTANCE && has_aggro_los) {
        enemy->aggroed = 1;
        enemy->attack_cooldown = fmaxf(enemy->attack_cooldown, bt3d_enemy_initial_attack_delay_for_class(app, enemy->class_id));
        play_sound_id(app, bt3d_enemy_aggro_sound_id(enemy->class_id), ENEMY_AGGRO_SOUND_VOLUME);
    } else if (enemy->aggroed && has_aggro_los && !enemy->had_los_last_tick) {
        enemy->attack_cooldown = fmaxf(enemy->attack_cooldown, bt3d_enemy_initial_attack_delay_for_class(app, enemy->class_id));
    }
    enemy->had_los_last_tick = has_aggro_los ? 1 : 0;
}

static void update_enemy_animation_from_state(EnemyRuntime *enemy, int moved_this_tick, float dt) {
    if (enemy->hurt_timer > 0.0f) {
        bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_PAIN);
    } else if (enemy->attack_timer > 0.0f) {
        bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_SHOOT);
    } else if (moved_this_tick) {
        bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_WALK);
    } else {
        enemy->anim_state = ENEMY_ANIM_WALK;
        enemy->anim_cursor = 0;
        enemy->anim_timer = 0.0f;
    }
    bt3d_advance_enemy_animation(enemy, dt);
}

static void update_enemy_chase_or_attack(AppState *app, EnemyRuntime *enemy, float distance, int has_direct_los, int self_index, Vector2 player_pos, float dt) {
    float attack_range;

    if (!enemy->aggroed || distance <= ENEMY_MIN_PLAYER_DISTANCE) return;

    attack_range = bt3d_enemy_attack_range_for_class(enemy->class_id);
    if (distance > attack_range || !has_direct_los) {
        Vector2 step = { 0 };
        if (!has_direct_los) {
            try_open_door_for_enemy(app, enemy, player_pos);
        }
        if (choose_path_step_from_distance_field(app, enemy, chase_distance_field(app), ENEMY_MOVE_SPEED * dt, &step)
            || choose_enemy_chase_step(app, enemy, ENEMY_MOVE_SPEED * dt, player_pos, self_index, &step)) {
            if (!move_enemy_with_collision(app, enemy, step, self_index)) {
                try_open_door_for_enemy(app, enemy, player_pos);
            }
        } else {
            try_open_door_for_enemy(app, enemy, player_pos);
        }
    } else if (enemy->attack_cooldown <= 0.0f && has_direct_los) {
        play_sound_id(app, bt3d_enemy_attack_sound_id(enemy->class_id), ENEMY_ATTACK_SOUND_VOLUME);
        if (bt3d_enemy_uses_projectile_for_class(enemy->class_id)) {
            bt3d_spawn_enemy_projectile(app, enemy);
        } else {
            bt3d_damage_player(app, bt3d_enemy_damage_for_class(app, enemy->class_id, distance));
        }
        enemy->attack_cooldown = bt3d_enemy_attack_cooldown_for_class(app, enemy->class_id);
        enemy->attack_timer = ENEMY_ATTACK_ANIMATION_TIME;
    }
}

void bt3d_update_enemies(AppState *app, float dt) {
    int i = 0;
    Vector2 player_pos = { app->player.position.x, app->player.position.z };

    for (i = 0; i < app->entities.enemy_count; ++i) {
        EnemyRuntime *enemy = &app->entities.enemies[i];
        Vector2 enemy_pos;
        Vector2 to_player;
        float distance;
        int has_aggro_los;
        int has_direct_los;
        int enemy_cell_x;
        int enemy_cell_y;
        float previous_x;
        float previous_z;
        int moved_this_tick = 0;
        int stunned_this_tick = 0;

        if (!enemy->alive) {
            bt3d_advance_enemy_animation(enemy, dt);
            continue;
        }
        enemy_pos = (Vector2){ enemy->position.x, enemy->position.z };
        if (Vector2DistanceSqr(enemy_pos, player_pos) > ENEMY_AI_ACTIVE_DISTANCE * ENEMY_AI_ACTIVE_DISTANCE) {
            update_enemy_timers(enemy, dt, 1);
            bt3d_advance_enemy_animation(enemy, dt);
            continue;
        }
        previous_x = enemy->position.x;
        previous_z = enemy->position.z;
        enemy_cell_x = (int)floorf(enemy->position.x);
        enemy_cell_y = (int)floorf(enemy->position.z);
        to_player = Vector2Subtract(player_pos, enemy_pos);
        distance = Vector2Length(to_player);
        has_aggro_los = bt3d_is_tile_visible_to_player(app, enemy_cell_x, enemy_cell_y);
        has_direct_los = has_aggro_los && bt3d_has_line_of_sight(app, enemy_pos, player_pos);
        stunned_this_tick = enemy->hurt_timer > 0.0f;
        update_enemy_timers(enemy, dt, 0);

        maybe_aggro_enemy(app, enemy, distance, has_aggro_los);

        if (stunned_this_tick) {
            bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_PAIN);
            bt3d_advance_enemy_animation(enemy, dt);
            continue;
        }

        update_enemy_chase_or_attack(app, enemy, distance, has_direct_los, i, player_pos, dt);

        moved_this_tick = fabsf(enemy->position.x - previous_x) > ENEMY_MOVED_EPSILON || fabsf(enemy->position.z - previous_z) > ENEMY_MOVED_EPSILON;
        update_enemy_animation_from_state(enemy, moved_this_tick, dt);
    }
}
