#ifndef BT3D_ASSETS_H
#define BT3D_ASSETS_H

#include "bt3d_types.h"

typedef struct AppState AppState;

/* Decodes every image and sound in the pack once; they stay loaded until exit. */
void bt3d_load_assets(AppState *app);
void bt3d_unload_assets(AppState *app);

/* Each returns NULL when the pack has no such image. */
const PackTexture *bt3d_texture(const AppState *app, const char *entry_name);
const PackTexture *bt3d_wall_texture(const AppState *app, int texture_id);
const PackTexture *bt3d_vec_texture(const AppState *app, int vec_id);

#endif
