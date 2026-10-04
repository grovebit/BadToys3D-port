#ifndef BT3D_PLATFORM_H
#define BT3D_PLATFORM_H

#include <stddef.h>

#include "datpack.h"

typedef struct AppState AppState;

const char *bt3d_save_directory(void);
/* Return 0 when the path does not fit. */
int bt3d_build_save_path(const char *file_name, char *out_path, size_t out_path_size);
int bt3d_build_quicksave_path(char *out_path, size_t out_path_size);
int bt3d_open_data_pack(DatPack *pack);
void bt3d_enter_game_directory(void);
void bt3d_reset_app_state(AppState *app);
void bt3d_init_window_and_audio(AppState *app);
void bt3d_toggle_display_mode(AppState *app);
void bt3d_save_config(const AppState *app);
void bt3d_load_config(AppState *app);
void bt3d_sync_persistent_storage(void);
/* Replace destination with a completed file on the same filesystem. */
int bt3d_replace_file(const char *source, const char *destination);

#endif
