#include "bt3d_app_state.h"
#include "bt3d_debug.h"
#include "bt3d_menu.h"
#include "bt3d_render.h"

#include "profiler.h"

static const Color WORLD_CLEAR_COLOR = { 14, 16, 18, 255 };

static void draw_missing_data_pack_screen(void) {
    const Color highlight = { 200, 200, 80, 255 };

    ClearBackground(WORLD_CLEAR_COLOR);
    DrawText("Bad Toys 3D", 40, 80, 36, (Color){ 255, 230, 160, 255 });
    DrawText("data.pck not found.", 40, 140, 22, RAYWHITE);
#if BT3D_PLATFORM_WEB
    DrawText("Reload the page and select the original data.pck.", 40, 180, 20, highlight);
#elif BT3D_PLATFORM_SWITCH
    DrawText("Copy the original data.pck to sdmc:/switch/bt3d/ and restart the game.", 40, 180, 20, highlight);
    DrawText("Press + to quit.", 40, 220, 18, GRAY);
#else
    DrawText("Copy the original data.pck to this folder and restart the game:", 40, 180, 20, highlight);
    DrawText(GetWorkingDirectory(), 40, 212, 18, LIGHTGRAY);
    DrawText("Press Esc to quit.", 40, 252, 18, GRAY);
#endif
}

static void draw_world_scene(AppState *app, Camera3D camera) {
    ClearBackground(WORLD_CLEAR_COLOR);
    BeginMode3D(camera);
    BT3D_PROF_BEGIN("draw_world");
    bt3d_draw_map_world(app, camera);
    BT3D_PROF_END("draw_world");
    BT3D_PROF_BEGIN("draw_billboards");
    bt3d_draw_billboard_sprites(app, camera);
    BT3D_PROF_END("draw_billboards");
    EndMode3D();
}

static void draw_death_transition_frame(AppState *app, Camera3D camera) {
    draw_world_scene(app, camera);
    bt3d_draw_real_hud(app);
    bt3d_draw_death_transition(app);
}

static void draw_world_frame(AppState *app, Camera3D camera) {
    draw_world_scene(app, camera);

    bt3d_draw_damage_indicator(app);
    BT3D_PROF_BEGIN("draw_hud");
    bt3d_draw_real_hud(app);
    bt3d_draw_player_weapon_viewmodel(app);
    BT3D_PROF_END("draw_hud");

    if (app->control.noclip_enabled) {
        DrawText("[NOCLIP]", 16, (int)bt3d_hud_rect(app).height + 8, 18, GRAY);
    }
    if (app->settings.debug_tools_enabled) {
        bt3d_draw_debug_overlay(app);
    }
    DrawText("+", GetScreenWidth() / 2 - 6, GetScreenHeight() / 2 - 14, 28, RAYWHITE);
}

static void draw_session_frame(AppState *app, Camera3D camera) {
    if (app->transition.death_active) {
        draw_death_transition_frame(app, camera);
    } else if (app->transition.active) {
        bt3d_draw_level_transition(app);
    } else if (app->control.map_overlay_active) {
        bt3d_draw_original_map_overlay(app);
        bt3d_draw_damage_indicator(app);
    } else {
        draw_world_frame(app, camera);
    }
}

void bt3d_draw_frame(AppState *app, Camera3D camera) {
    BeginDrawing();
    if (app->content.pack.entry_count == 0) {
        draw_missing_data_pack_screen();
    } else if (app->menu.active || !app->session.in_session) {
        bt3d_draw_menu(app);
    } else {
        draw_session_frame(app, camera);
    }
    bt3d_profiler_end_frame();
    EndDrawing();
}
