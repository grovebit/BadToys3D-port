#ifndef BT3D_SESSION_H
#define BT3D_SESSION_H

typedef struct AppState AppState;

int bt3d_load_initial_pack(AppState *app);
int start_new_game(AppState *app);
/* Loads map `index` with no doors, pickups or enemies yet; anything left
   from the previous map is cleared on success. Failure preserves it. */
int bt3d_load_map(AppState *app, int index);
/* Loads map `index` with its doors, pickups, enemies and the player as the map places them. */
int bt3d_start_map(AppState *app, int index);
void bt3d_free_entities(AppState *app);
void bt3d_cycle_map(AppState *app, int delta);
void bt3d_begin_level_transition(AppState *app, int next_index);

#endif
