#ifndef BT3D_WEAPON_H
#define BT3D_WEAPON_H

typedef struct AppState AppState;

typedef struct {
    int uses_ammo;
    int ammo_slot;
    int ammo_cost;
    int attack_sequence_length;
    float attack_range;
    int damage;
    int projectile;
    int fire_sound_id;
    float fire_volume;
    int idle_frame;
    const int *fire_frames;
    int fire_frame_count;
} Bt3dPlayerWeaponDef;

const Bt3dPlayerWeaponDef *bt3d_player_weapon_def(int weapon_slot);
float bt3d_player_weapon_attack_timer_seconds(int weapon_slot);
float bt3d_player_weapon_refire_seconds(int weapon_slot);
int bt3d_player_weapon_current_ammo(const AppState *app);
int bt3d_player_weapon_anim_frame(int weapon_slot, float attack_timer);
float bt3d_player_weapon_kick(int weapon_slot, float attack_timer);

#endif
