#ifndef BT3D_PLAYER_INTERNAL_H
#define BT3D_PLAYER_INTERNAL_H

#include "bt3d_player.h"

int bt3d_player_cycle_weapon(AppState *app, int direction);

void bt3d_player_try_use(AppState *app);
void bt3d_player_trigger_remote_descriptor_events(AppState *app);
void bt3d_player_collect_markers(AppState *app);

void bt3d_player_apply_movement(AppState *app, Vector3 move, float dt);

/* Runs the death and level transitions; returns whether one took the frame. */
int bt3d_player_update_transitions(AppState *app, float dt);
void bt3d_player_update_timers(AppState *app, float dt);
void bt3d_player_update_gameplay_systems(AppState *app, float dt);
void bt3d_player_update_ambient_sounds(AppState *app, float dt);

#endif
