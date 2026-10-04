#ifndef BT3D_SAVEGAME_H
#define BT3D_SAVEGAME_H

typedef struct AppState AppState;

int bt3d_save_game_to_file(AppState *app, const char *path);
int load_game_from_file(AppState *app, const char *path);

#endif
