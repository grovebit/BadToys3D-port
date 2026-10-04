#include "bt3d_app_state.h"
#include "bt3d_weapon.h"

#include "bt3d_math.h"

#include <math.h>

static const int fist_fire[] = { 137, 136 };
static const int pistol_fire[] = { 132, 133, 134 };
static const int weapon2_fire[] = { 143, 144, 145, 146 };
static const int weapon3_fire[] = { 139, 140, 141, 150, 151 };

#define WEAPON_FRAMES(frames) frames, ARRAY_COUNT(frames)

static const Bt3dPlayerWeaponDef player_weapon_catalog[] = {
    { 0 },
    { 0, 0, 0, 3, 1.35f, 3,  0, 5, 0.60f, 135, WEAPON_FRAMES(fist_fire) },
    { 1, 2, 1, 4, INFINITY, 6,  0, 1, 0.70f, 131, WEAPON_FRAMES(pistol_fire) },
    { 1, 3, 1, 5, INFINITY, 10, 0, 2, 0.72f, 142, WEAPON_FRAMES(weapon2_fire) },
    { 1, 4, 5, 6, INFINITY, 30, 1, 3, 0.78f, 138, WEAPON_FRAMES(weapon3_fire) },
};

#undef WEAPON_FRAMES

const Bt3dPlayerWeaponDef *bt3d_player_weapon_def(int weapon_slot) {
    if (weapon_slot >= 1 && weapon_slot < ARRAY_COUNT(player_weapon_catalog)) {
        return &player_weapon_catalog[weapon_slot];
    }
    return &player_weapon_catalog[2];
}

float bt3d_player_weapon_attack_timer_seconds(int weapon_slot) {
    const Bt3dPlayerWeaponDef *def = bt3d_player_weapon_def(weapon_slot);
    return ((float)(bt3d_max_i(1, def->attack_sequence_length) + 1) * GAME_TICK_SECS);
}

float bt3d_player_weapon_refire_seconds(int weapon_slot) {
    const Bt3dPlayerWeaponDef *def = bt3d_player_weapon_def(weapon_slot);
    return ((float)(bt3d_max_i(1, def->attack_sequence_length) + 3) * GAME_TICK_SECS);
}

int bt3d_player_weapon_current_ammo(const AppState *app) {
    const Bt3dPlayerWeaponDef *def;
    def = bt3d_player_weapon_def(app->player.current_weapon_slot);
    return def->uses_ammo ? app->player.weapon_ammo[def->ammo_slot] : app->player.weapon_ammo[2];
}

/* 0 when an attack starts, 1 when its animation has finished. */
static float attack_progress(float attack_timer, float attack_duration) {
    return 1.0f - bt3d_clamp01(attack_timer / attack_duration);
}

int bt3d_player_weapon_anim_frame(int weapon_slot, float attack_timer) {
    const Bt3dPlayerWeaponDef *def = bt3d_player_weapon_def(weapon_slot);
    float attack_duration = bt3d_player_weapon_attack_timer_seconds(weapon_slot);
    int frame_index;

    if (attack_timer <= 0.0f || def->fire_frame_count <= 0 || attack_duration <= 0.0f) {
        return def->idle_frame;
    }

    frame_index = (int)floorf(attack_progress(attack_timer, attack_duration) * (float)def->fire_frame_count);
    return def->fire_frames[bt3d_clamp_i(frame_index, 0, def->fire_frame_count - 1)];
}

float bt3d_player_weapon_kick(int weapon_slot, float attack_timer) {
    float attack_duration = bt3d_player_weapon_attack_timer_seconds(weapon_slot);
    if (attack_timer <= 0.0f || attack_duration <= 0.0f) return 0.0f;
    return sinf(attack_progress(attack_timer, attack_duration) * PI);
}
