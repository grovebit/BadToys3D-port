#ifndef BT3D_WORLD_DOORS_H
#define BT3D_WORLD_DOORS_H

typedef struct AppState AppState;

int door_slides_on_x_axis(const AppState *app, int cell_x, int cell_y);
float door_openness_at(const AppState *app, int x, int y);

#endif
