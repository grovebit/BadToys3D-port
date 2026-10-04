#ifndef BT3D_WORLD_TEXTURES_H
#define BT3D_WORLD_TEXTURES_H

typedef struct AppState AppState;

int wall_texture_index_for_face(const AppState *app, int cell_x, int cell_y, int face_index);
int floor_texture_index_for_cell(const AppState *app, int cell_x, int cell_y);
int ceiling_texture_index_for_cell(const AppState *app, int cell_x, int cell_y);
int door_overlay_index_for_cell(const AppState *app, int cell_x, int cell_y);
int wall_overlay_index_for_face(const AppState *app, int cell_x, int cell_y, int face_index);

#endif
