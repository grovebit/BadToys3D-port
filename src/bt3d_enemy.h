#ifndef BT3D_ENEMY_H
#define BT3D_ENEMY_H

#include "bt3d_types.h"
#include "map242.h"

#include <stddef.h>

typedef struct AppState AppState;

/* Sprite classes 0-5 are enemies, 6-8 their projectiles, 9 the player's shot impact. */
enum {
    BT3D_ENEMY_CLASS_COUNT = 6,
    BT3D_SPRITE_CLASS_SHOT_IMPACT = 9,
    BT3D_SPRITE_CLASS_COUNT = 10
};

int bt3d_enemy_aggro_sound_id(int class_id);
int bt3d_enemy_attack_sound_id(int class_id);
int bt3d_enemy_death_sound_id(int class_id);
int bt3d_drop_marker_type_for_enemy_class(int class_id);
void bt3d_sprite_entry_name_for_class(int class_id, int frame, char *out, size_t out_size);
int bt3d_is_sprite_entry_name(const char *name);
float bt3d_enemy_attack_range_for_class(int class_id);
int bt3d_enemy_damage_for_class(const AppState *app, int class_id, float distance);
float bt3d_enemy_attack_cooldown_for_class(const AppState *app, int class_id);
float bt3d_enemy_initial_attack_delay_for_class(const AppState *app, int class_id);
int bt3d_enemy_uses_projectile_for_class(int class_id);
const char *bt3d_difficulty_name(int difficulty);
int bt3d_enemy_projectile_class_for_enemy_class(int class_id);
int bt3d_enemy_projectile_damage_for_class(int class_id);
float bt3d_enemy_projectile_blast_radius_for_class(int class_id);
int bt3d_enemy_projectile_frame(int projectile_class_id, float animation_timer);
int bt3d_enemy_idle_frame_for_class(int class_id);
void bt3d_set_enemy_animation_state(EnemyRuntime *enemy, int next_state);
void bt3d_advance_enemy_animation(EnemyRuntime *enemy, float dt);
void bt3d_init_enemy_runtime(const AppState *app, EnemyRuntime *enemy, const MapObject *object, int spawn_index);
void bt3d_update_enemies(AppState *app, float dt);

#endif
