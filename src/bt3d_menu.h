#ifndef BT3D_MENU_H
#define BT3D_MENU_H

#include "bt3d_input.h"

typedef struct AppState AppState;

/* Shows the main menu and releases the mouse. */
void bt3d_open_menu(AppState *app);
void bt3d_update_menu(AppState *app, const FrameInput *input);
void bt3d_draw_menu(const AppState *app);
void bt3d_update_menu_music(AppState *app);

#endif
