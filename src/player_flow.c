#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_combat.h"
#include "bt3d_doors.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_save_slots.h"
#include "bt3d_session.h"
#include "bt3d_world_overlays.h"
#include "bt3d_world_spawn.h"
#include "bt3d_world_visibility.h"
#include "profiler.h"
#include "player_internal.h"

#include <math.h>
#include <string.h>

/* Restarting a level keeps the fist and pistol and loses the rest. */
static void reset_player_for_current_level(AppState *app) {
    app->player.health = 99;
    app->player.weapon_slots[3] = 0;
    app->player.weapon_slots[4] = 0;
    app->player.weapon_ammo[3] = 0;
    app->player.weapon_ammo[4] = 0;
    app->player.weapon_ammo[2] = 24;
    memset(app->player.keys, 0, sizeof(app->player.keys));
    if (app->player.current_weapon_slot == 3 || app->player.current_weapon_slot == 4) {
        app->player.current_weapon_slot = 2;
    }
}

int bt3d_player_update_transitions(AppState *app, float dt) {
    if (app->transition.death_active) {
        app->transition.death_timer -= dt;
        if (app->transition.death_timer <= 0.0f) {
            /* Out of lives: back to the first level with a fresh player. */
            int game_over = app->player.lives <= 0;
            if (game_over) {
                init_player_state(app);
            } else {
                reset_player_for_current_level(app);
            }
            bt3d_start_map(app, game_over ? 0 : app->session.current_map_index);
        }
        return 1;
    }

    if (app->transition.active) {
        app->transition.timer -= dt;
        if (app->transition.timer <= 0.0f) {
            bt3d_start_map(app, app->transition.next_index);
            memset(app->player.keys, 0, sizeof(app->player.keys));
            bt3d_save_game_to_latest_slot(app);
        }
        return 1;
    }

    return 0;
}

void bt3d_player_update_timers(AppState *app, float dt) {
    app->player.shot_cooldown = fmaxf(0.0f, app->player.shot_cooldown - dt);
    app->player.attack_timer = fmaxf(0.0f, app->player.attack_timer - dt);
    app->transition.damage_flash_timer = fmaxf(0.0f, app->transition.damage_flash_timer - dt);
}

void bt3d_player_update_gameplay_systems(AppState *app, float dt) {
    bt3d_update_overlay_animations(app, dt);
    BT3D_PROF_BEGIN("events");
    bt3d_update_events(app, dt);
    bt3d_player_trigger_remote_descriptor_events(app);
    bt3d_player_collect_markers(app);
    BT3D_PROF_END("events");
    BT3D_PROF_BEGIN("visibility");
    bt3d_refresh_visible_tile_mask(app);
    BT3D_PROF_END("visibility");
    BT3D_PROF_BEGIN("projectiles");
    bt3d_update_player_projectiles(app, dt);
    bt3d_update_enemy_projectiles(app, dt);
    bt3d_update_player_shot_impacts(app, dt);
    BT3D_PROF_END("projectiles");
    BT3D_PROF_BEGIN("enemies");
    bt3d_update_enemies(app, dt);
    BT3D_PROF_END("enemies");
}

void bt3d_player_update_ambient_sounds(AppState *app, float dt) {
    static const int ambient_pool[] = { 16, 17, 30, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41 };
    int pick;

    app->audio.ambient_timer -= dt;
    if (app->audio.ambient_timer > 0.0f) return;
    pick = ambient_pool[GetRandomValue(0, ARRAY_COUNT(ambient_pool) - 1)];
    if (pick == app->audio.last_ambient_sound) {
        pick = ambient_pool[GetRandomValue(0, ARRAY_COUNT(ambient_pool) - 1)];
    }
    play_sound_id(app, pick, 0.34f);
    app->audio.last_ambient_sound = pick;
    app->audio.ambient_timer = 45.0f + (float)GetRandomValue(0, 15);
}
