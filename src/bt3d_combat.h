#ifndef BT3D_COMBAT_H
#define BT3D_COMBAT_H

#include "bt3d_types.h"

typedef struct AppState AppState;

void bt3d_damage_player(AppState *app, int amount);
void bt3d_spawn_enemy_projectile(AppState *app, const EnemyRuntime *enemy);
void bt3d_update_player_projectiles(AppState *app, float dt);
void bt3d_update_enemy_projectiles(AppState *app, float dt);
void bt3d_update_player_shot_impacts(AppState *app, float dt);
void bt3d_fire_player_weapon(AppState *app);

#endif
