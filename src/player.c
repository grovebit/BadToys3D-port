#include "bt3d_app_state.h"
#include "bt3d_combat.h"
#include "bt3d_debug.h"
#include "bt3d_menu.h"
#include "bt3d_player.h"
#include "bt3d_save_slots.h"
#include "bt3d_session.h"
#include "player_internal.h"
#include "profiler.h"

#include "raymath.h"

#include <math.h>

static const float PLAYER_KEYBOARD_TURN_SPEED = 2.4f;
/* Radians per second at full stick, before the sensitivity setting. */
static const float PLAYER_GAMEPAD_TURN_SPEED = 4.7f;

Vector3 bt3d_player_forward(const AppState *app) {
    return (Vector3){ sinf(app->player.yaw), 0.0f, cosf(app->player.yaw) };
}

Vector2 bt3d_player_forward_xz(const AppState *app) {
    return (Vector2){ sinf(app->player.yaw), cosf(app->player.yaw) };
}

void bt3d_update_camera_target(Camera3D *camera, const AppState *app) {
    camera->position = app->player.position;
    camera->target = Vector3Add(app->player.position, bt3d_player_forward(app));
}

static void update_look(AppState *app, const FrameInput *input, float dt) {
    float sensitivity = app->settings.mouse_sensitivity;

    if (app->control.mouse_captured) app->player.yaw -= input->mouse_delta.x * TURN_SPEED * sensitivity;
    app->player.yaw += input->turn * PLAYER_KEYBOARD_TURN_SPEED * dt;
    app->player.yaw -= input->look * fabsf(input->look) * PLAYER_GAMEPAD_TURN_SPEED * sensitivity * dt;
}

static void handle_debug_actions(AppState *app, const FrameInput *input) {
    if (!app->settings.debug_tools_enabled) return;
    if (input->noclip) app->control.noclip_enabled = !app->control.noclip_enabled;
    if (input->profiler) bt3d_profiler_toggle();
    if (input->copy_debug_text) bt3d_copy_debug_target_text(app);
    if (input->prev_map) bt3d_cycle_map(app, -1);
    if (input->next_map) bt3d_cycle_map(app, 1);
}

static void handle_actions(AppState *app, const FrameInput *input) {
    handle_debug_actions(app, input);
    if (input->map) {
        app->control.map_overlay_active = !app->control.map_overlay_active;
    } else if (input->moved) {
        app->control.map_overlay_active = 0;
    }
    if (input->quicksave) bt3d_save_game_to_latest_slot(app);
    if (input->quickload) bt3d_load_game_from_latest_slot(app);
    if (input->weapon_slot && bt3d_player_weapon_selectable(app, input->weapon_slot)) {
        app->player.current_weapon_slot = input->weapon_slot;
    }
    if (input->prev_weapon) bt3d_player_cycle_weapon(app, -1);
    if (input->next_weapon) bt3d_player_cycle_weapon(app, 1);
    if (input->use) bt3d_player_try_use(app);
    if (input->fire) bt3d_fire_player_weapon(app);
}

static void update_gameplay(AppState *app, const FrameInput *input, float dt) {
    Vector3 forward;
    Vector3 strafe_right;

    if (input->pause) {
        bt3d_open_menu(app);
        return;
    }
    update_look(app, input, dt);
    handle_actions(app, input);

    forward = bt3d_player_forward(app);
    strafe_right = (Vector3){ -forward.z, 0.0f, forward.x };
    bt3d_player_apply_movement(app, Vector3Add(Vector3Scale(forward, input->move.y), Vector3Scale(strafe_right, input->move.x)), dt);

    bt3d_player_update_timers(app, dt);
    bt3d_player_update_gameplay_systems(app, dt);
    bt3d_player_update_ambient_sounds(app, dt);
}

void bt3d_update_player(AppState *app, const FrameInput *input, Camera3D *camera) {
    float dt = GetFrameTime();

    if (app->menu.active) {
        bt3d_update_menu(app, input);
    } else if (!bt3d_player_update_transitions(app, dt)) {
        update_gameplay(app, input, dt);
    }
    bt3d_update_camera_target(camera, app);
}
