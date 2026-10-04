#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_combat.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_pickup.h"
#include "bt3d_player.h"
#include "bt3d_weapon.h"
#include "bt3d_world_visibility.h"

#include "raymath.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A projectile explodes within the trigger radius of the player and, without
   a blast radius, only damages within the slightly larger hit radius. */
static const float ENEMY_PROJECTILE_TRIGGER_RADIUS = 0.45f;
static const float ENEMY_PROJECTILE_HIT_RADIUS = 0.5f;

typedef struct {
    float wall_distance;
    Vector2 impact;
    int hit_wall;
} PlayerShotTrace;

static void spawn_drop_for_enemy(AppState *app, const EnemyRuntime *enemy) {
    int marker_type;
    int cell_x;
    int cell_y;
    int i = 0;
    MarkerRuntime *expanded;
    MarkerRuntime *marker;

    marker_type = bt3d_drop_marker_type_for_enemy_class(enemy->class_id);
    if (marker_type == 0) return;

    cell_x = (int)floorf(enemy->position.x);
    cell_y = (int)floorf(enemy->position.z);
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return;

    for (i = 0; i < app->entities.marker_count; ++i) {
        MarkerRuntime *existing = &app->entities.markers[i];
        if (existing->collected) continue;
        if (existing->base.cell_x == cell_x && existing->base.cell_y == cell_y) {
            return;
        }
    }

    expanded = (MarkerRuntime *)realloc(app->entities.markers, sizeof(MarkerRuntime) * (size_t)(app->entities.marker_count + 1));
    if (!expanded) return;
    app->entities.markers = expanded;
    marker = &app->entities.markers[app->entities.marker_count];
    memset(marker, 0, sizeof(*marker));
    marker->base.type = (uint16_t)marker_type;
    marker->base.cell_x = (uint8_t)cell_x;
    marker->base.cell_y = (uint8_t)cell_y;
    app->entities.marker_count++;
}

static void kill_enemy(AppState *app, EnemyRuntime *enemy) {
    enemy->alive = 0;
    enemy->position.y = 0.09f;
    bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_DIE);
    play_sound_id(app, bt3d_enemy_death_sound_id(enemy->class_id), 0.7f);
    spawn_drop_for_enemy(app, enemy);
}

/* Kills the enemy or starts its pain reaction; returns whether it died. */
static int damage_enemy(AppState *app, EnemyRuntime *enemy, int damage) {
    enemy->health -= damage;
    if (enemy->health <= 0) {
        kill_enemy(app, enemy);
        return 1;
    }
    enemy->hurt_timer = bt3d_player_weapon_refire_seconds(2);
    bt3d_set_enemy_animation_state(enemy, ENEMY_ANIM_PAIN);
    return 0;
}

void bt3d_damage_player(AppState *app, int amount) {
    if (amount <= 0 || app->transition.death_active || app->control.god_mode_enabled) return;
    app->player.health -= amount;
    app->transition.damage_flash_timer = fminf(0.45f, app->transition.damage_flash_timer + 0.22f);
    if (app->player.health > 0) return;
    app->player.health = 0;
    app->player.lives = bt3d_max_i(app->player.lives - 1, 0);
    app->transition.death_active = 1;
    app->transition.death_timer = 1.1f;
}

/* Player rocket damage: linear falloff over 95 original map units, at least 1. */
static int player_blast_damage(int max_damage, float distance) {
    return bt3d_max_i(1, (int)floorf(((float)max_damage * fmaxf(0.0f, 95.0f - distance * 64.0f)) / 95.0f));
}

static void apply_player_projectile_explosion(AppState *app, const PlayerProjectile *projectile, Vector2 impact) {
    int i = 0;
    float player_distance;

    for (i = 0; i < app->entities.enemy_count; ++i) {
        EnemyRuntime *enemy = &app->entities.enemies[i];
        float distance;
        if (!enemy->alive) continue;
        distance = Vector2Distance((Vector2){ enemy->position.x, enemy->position.z }, impact);
        if (distance > projectile->blast_radius) continue;
        damage_enemy(app, enemy, player_blast_damage(projectile->max_damage, distance));
    }

    player_distance = Vector2Distance((Vector2){ app->player.position.x, app->player.position.z }, impact);
    if (player_distance <= projectile->blast_radius) {
        bt3d_damage_player(app, player_blast_damage(projectile->max_damage, player_distance));
    }
}

static PlayerShotTrace trace_player_shot_wall(AppState *app, Vector2 origin, Vector2 direction, float max_range) {
    Bt3dGridRay ray;
    float hit_distance = max_range;
    PlayerShotTrace result = {
        .wall_distance = max_range,
        .impact = Vector2Add(origin, Vector2Scale(direction, max_range)),
        .hit_wall = 0,
    };

    ray = bt3d_grid_ray_begin(origin, direction);
    if (max_range <= 0.0f || bt3d_is_visibility_blocked_cell(app, ray.map_x, ray.map_y)) {
        result.wall_distance = 0.0f;
        result.impact = origin;
        result.hit_wall = 1;
        return result;
    }

    while (hit_distance >= 0.0f) {
        hit_distance = bt3d_grid_ray_step(&ray);
        if (hit_distance > max_range) break;
        if (bt3d_is_visibility_blocked_cell(app, ray.map_x, ray.map_y)) {
            result.wall_distance = hit_distance;
            result.impact = Vector2Add(origin, Vector2Scale(direction, hit_distance));
            result.hit_wall = 1;
            return result;
        }
    }

    return result;
}

static void spawn_player_shot_impact(AppState *app, Vector2 impact, Vector2 direction) {
    PlayerShotImpact *slot = NULL;

    if (app->projectiles.shot_impact_count >= MAX_PLAYER_SHOT_IMPACTS) return;
    slot = &app->projectiles.shot_impacts[app->projectiles.shot_impact_count++];
    slot->position = (Vector3){
        impact.x - direction.x * 0.025f,
        0.46f,
        impact.y - direction.y * 0.025f,
    };
    slot->age = 0.0f;
    slot->life = GAME_TICK_SECS * 3.0f;
}

static void spawn_player_projectile(AppState *app, const Bt3dPlayerWeaponDef *weapon) {
    PlayerProjectile *projectile;
    Vector3 forward;

    if (app->projectiles.player_count >= MAX_PLAYER_PROJECTILES) return;
    projectile = &app->projectiles.player[app->projectiles.player_count++];
    forward = bt3d_player_forward(app);
    projectile->position = (Vector3){
        app->player.position.x + forward.x * 0.42f,
        0.45f,
        app->player.position.z + forward.z * 0.42f
    };
    projectile->velocity = Vector3Scale(forward, PLAYER_PROJECTILE_SPEED);
    projectile->max_damage = weapon->damage;
    projectile->blast_radius = PLAYER_PROJECTILE_BLAST_RADIUS;
    projectile->life = 2.2f;
}

static void apply_enemy_projectile_explosion(AppState *app, const EnemyProjectile *projectile, Vector2 impact) {
    float player_distance;

    player_distance = Vector2Distance((Vector2){ app->player.position.x, app->player.position.z }, impact);
    if (projectile->blast_radius <= 0.0f) {
        if (player_distance <= ENEMY_PROJECTILE_HIT_RADIUS) {
            bt3d_damage_player(app, projectile->max_damage);
        }
        return;
    }
    if (player_distance > projectile->blast_radius) return;

    {
        float falloff = fmaxf(0.0f, 1.0f - player_distance / projectile->blast_radius);
        int damage = bt3d_max_i(1, (int)floorf((float)projectile->max_damage * falloff));
        bt3d_damage_player(app, damage);
    }
}

void bt3d_spawn_enemy_projectile(AppState *app, const EnemyRuntime *enemy) {
    EnemyProjectile *projectile;
    Vector2 to_player;
    float distance;
    int projectile_class_id;

    if (app->projectiles.enemy_count >= MAX_ENEMY_PROJECTILES) return;
    projectile_class_id = bt3d_enemy_projectile_class_for_enemy_class(enemy->class_id);
    if (projectile_class_id < 0) return;

    to_player = Vector2Subtract((Vector2){ app->player.position.x, app->player.position.z }, (Vector2){ enemy->position.x, enemy->position.z });
    distance = Vector2Length(to_player);
    if (distance <= 1.0e-5f) return;

    projectile = &app->projectiles.enemy[app->projectiles.enemy_count++];
    projectile->position = (Vector3){ enemy->position.x, 0.45f, enemy->position.z };
    projectile->velocity = (Vector3){ (to_player.x / distance) * 10.0f, 0.0f, (to_player.y / distance) * 10.0f };
    projectile->projectile_class_id = projectile_class_id;
    projectile->max_damage = bt3d_enemy_projectile_damage_for_class(enemy->class_id);
    projectile->blast_radius = bt3d_enemy_projectile_blast_radius_for_class(enemy->class_id);
    projectile->life = 2.2f;
    projectile->animation_timer = 0.0f;
}

void bt3d_update_player_projectiles(AppState *app, float dt) {
    int write_index = 0;
    int i = 0;

    for (i = 0; i < app->projectiles.player_count; ++i) {
        PlayerProjectile projectile = app->projectiles.player[i];
        float distance = Vector3Length(projectile.velocity) * dt;
        int steps = bt3d_max_i(1, (int)ceilf(distance / 0.1f));
        float step_fraction = 1.0f / (float)steps;
        int exploded = 0;
        int step_i = 0;

        for (step_i = 0; step_i < steps; ++step_i) {
            int enemy_i = 0;
            projectile.position = Vector3Add(projectile.position, Vector3Scale(projectile.velocity, dt * step_fraction));
            if (bt3d_is_visibility_blocked_cell(app, (int)floorf(projectile.position.x), (int)floorf(projectile.position.z))) {
                apply_player_projectile_explosion(app, &projectile, (Vector2){ projectile.position.x, projectile.position.z });
                exploded = 1;
                break;
            }
            for (enemy_i = 0; enemy_i < app->entities.enemy_count; ++enemy_i) {
                EnemyRuntime *enemy = &app->entities.enemies[enemy_i];
                if (!enemy->alive) continue;
                if (Vector2DistanceSqr((Vector2){ projectile.position.x, projectile.position.z },
                                       (Vector2){ enemy->position.x, enemy->position.z }) <= 0.28f * 0.28f) {
                    apply_player_projectile_explosion(app, &projectile, (Vector2){ projectile.position.x, projectile.position.z });
                    exploded = 1;
                    break;
                }
            }
            if (exploded) break;
        }

        if (exploded) continue;
        projectile.life -= dt;
        if (projectile.life > 0.0f && write_index < MAX_PLAYER_PROJECTILES) {
            app->projectiles.player[write_index++] = projectile;
        }
    }

    app->projectiles.player_count = write_index;
}

void bt3d_update_player_shot_impacts(AppState *app, float dt) {
    int write_index = 0;
    int i = 0;

    for (i = 0; i < app->projectiles.shot_impact_count; ++i) {
        PlayerShotImpact impact = app->projectiles.shot_impacts[i];
        impact.age += dt;
        if (impact.age < impact.life && write_index < MAX_PLAYER_SHOT_IMPACTS) {
            app->projectiles.shot_impacts[write_index++] = impact;
        }
    }
    app->projectiles.shot_impact_count = write_index;
}

void bt3d_update_enemy_projectiles(AppState *app, float dt) {
    int i = 0;
    int write_index = 0;

    for (i = 0; i < app->projectiles.enemy_count; ++i) {
        EnemyProjectile projectile = app->projectiles.enemy[i];
        float distance = Vector3Length(projectile.velocity) * dt;
        int steps = bt3d_max_i(1, (int)ceilf(distance / 0.05f));
        float step_fraction = dt / (float)steps;
        int exploded = 0;
        int step = 0;

        for (step = 0; step < steps; ++step) {
            projectile.position = Vector3Add(projectile.position, Vector3Scale(projectile.velocity, step_fraction));
            if (bt3d_is_visibility_blocked_cell(app, (int)floorf(projectile.position.x), (int)floorf(projectile.position.z))) {
                apply_enemy_projectile_explosion(app, &projectile, (Vector2){ projectile.position.x, projectile.position.z });
                exploded = 1;
                break;
            }
            if (Vector2DistanceSqr((Vector2){ app->player.position.x, app->player.position.z },
                    (Vector2){ projectile.position.x, projectile.position.z }) <= ENEMY_PROJECTILE_TRIGGER_RADIUS * ENEMY_PROJECTILE_TRIGGER_RADIUS) {
                apply_enemy_projectile_explosion(app, &projectile, (Vector2){ projectile.position.x, projectile.position.z });
                exploded = 1;
                break;
            }
        }

        if (exploded) continue;
        projectile.life -= dt;
        projectile.animation_timer += dt;
        if (projectile.life > 0.0f && write_index < MAX_ENEMY_PROJECTILES) {
            app->projectiles.enemy[write_index++] = projectile;
        }
    }

    app->projectiles.enemy_count = write_index;
}

static int find_player_weapon_target(AppState *app, float max_range, PlayerShotTrace *out_trace, Vector2 *out_forward) {
    int best_index = -1;
    float best_hit_distance;
    int i = 0;
    Vector2 forward;
    Vector2 camera_pos;
    const float hit_radius = 0.32f;
    float trace_range = isinf(max_range) ? 96.0f : max_range;
    PlayerShotTrace trace;

    forward = bt3d_player_forward_xz(app);
    camera_pos = (Vector2){ app->player.position.x, app->player.position.z };
    trace = trace_player_shot_wall(app, camera_pos, forward, trace_range);
    if (out_forward) *out_forward = forward;
    best_hit_distance = trace.wall_distance;

    for (i = 0; i < app->entities.enemy_count; ++i) {
        EnemyRuntime *enemy = &app->entities.enemies[i];
        Vector2 to_enemy;
        float along;
        float perpendicular_sq;
        float hit_offset;
        float enemy_hit_distance;
        if (!enemy->alive) continue;
        to_enemy = Vector2Subtract((Vector2){ enemy->position.x, enemy->position.z }, camera_pos);
        along = Vector2DotProduct(forward, to_enemy);
        if (along > trace_range || along < 0.001f) continue;
        perpendicular_sq = Vector2LengthSqr(to_enemy) - along * along;
        if (perpendicular_sq > hit_radius * hit_radius) continue;
        hit_offset = sqrtf(fmaxf(0.0f, hit_radius * hit_radius - perpendicular_sq));
        enemy_hit_distance = along - hit_offset;
        if (enemy_hit_distance < 0.001f) enemy_hit_distance = along;
        if (enemy_hit_distance < best_hit_distance) {
            best_hit_distance = enemy_hit_distance;
            best_index = i;
        }
    }

    if (best_index >= 0) {
        trace.impact = Vector2Add(camera_pos, Vector2Scale(forward, best_hit_distance));
        trace.hit_wall = 0;
    }
    if (out_trace) *out_trace = trace;

    return best_index;
}

void bt3d_fire_player_weapon(AppState *app) {
    int best_index = -1;
    int fired_slot;
    const Bt3dPlayerWeaponDef *weapon;
    PlayerShotTrace shot_trace;
    Vector2 shot_forward = { 0.0f, 1.0f };

    if (app->player.shot_cooldown > 0.0f) return;

    if (!bt3d_player_weapon_selectable(app, app->player.current_weapon_slot)) {
        bt3d_select_previous_available_weapon(app);
    }
    fired_slot = app->player.current_weapon_slot;
    weapon = bt3d_player_weapon_def(app->player.current_weapon_slot);
    if (weapon->uses_ammo && app->player.weapon_ammo[weapon->ammo_slot] < weapon->ammo_cost) {
        bt3d_select_previous_available_weapon(app);
        return;
    }
    if (weapon->uses_ammo) {
        app->player.weapon_ammo[weapon->ammo_slot] = bt3d_max_i(0, app->player.weapon_ammo[weapon->ammo_slot] - weapon->ammo_cost);
        if (app->player.weapon_ammo[weapon->ammo_slot] <= 0 && app->player.current_weapon_slot == fired_slot) {
            bt3d_select_previous_available_weapon(app);
        }
    }
    app->player.shot_cooldown = bt3d_player_weapon_refire_seconds(fired_slot);
    app->player.attack_timer = bt3d_player_weapon_attack_timer_seconds(fired_slot);
    play_sound_id(app, weapon->fire_sound_id, weapon->fire_volume);

    if (weapon->projectile) {
        spawn_player_projectile(app, weapon);
        return;
    }

    best_index = find_player_weapon_target(app, weapon->attack_range, &shot_trace, &shot_forward);
    if (best_index < 0) {
        if (shot_trace.hit_wall) {
            spawn_player_shot_impact(app, shot_trace.impact, shot_forward);
        }
        return;
    }

    spawn_player_shot_impact(app, shot_trace.impact, shot_forward);
    damage_enemy(app, &app->entities.enemies[best_index], weapon->damage);
}
