#ifndef BT3D_SAVE_SLOTS_H
#define BT3D_SAVE_SLOTS_H

#include <stddef.h>

typedef struct AppState AppState;

int bt3d_save_game_to_latest_slot(AppState *app);
int bt3d_load_game_from_latest_slot(AppState *app);
void refresh_save_entries(AppState *app);
int save_game_to_named_slot(AppState *app, const char *name);
int delete_save_file(AppState *app, const char *path);
void default_save_name_with_level(AppState *app, char *out_name, size_t out_name_size);

#endif
