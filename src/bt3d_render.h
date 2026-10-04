#ifndef BT3D_RENDER_H
#define BT3D_RENDER_H

#include "raylib.h"

typedef struct AppState AppState;

void bt3d_draw_frame(AppState *app, Camera3D camera);
Rectangle bt3d_hud_rect(const AppState *app);
void bt3d_draw_player_weapon_viewmodel(const AppState *app);
void bt3d_draw_real_hud(const AppState *app);
int bt3d_draw_pod_background(const AppState *app);
void bt3d_draw_level_transition(const AppState *app);
void bt3d_draw_death_transition(const AppState *app);
void bt3d_draw_damage_indicator(const AppState *app);
void bt3d_draw_map_world(AppState *app, Camera3D camera);
void bt3d_draw_billboard_sprites(AppState *app, Camera3D camera);

/* The camera's horizontal field of view, for skipping what lies outside it. */
typedef struct {
    Vector2 origin;
    Vector2 edge_normals[2];
    int culling;
} Bt3dViewCone;

Bt3dViewCone bt3d_view_cone(Camera3D camera);
/* Whether a circle of `radius` around (x, z) can overlap the view. */
int bt3d_view_cone_contains(const Bt3dViewCone *cone, float x, float z, float radius);
void bt3d_draw_original_map_overlay(AppState *app);

#endif
