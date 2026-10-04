#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_doors.h"
#include "bt3d_pickup.h"
#include "bt3d_session.h"
#include "bt3d_world_classify.h"
#include "bt3d_world_markers.h"
#include "bt3d_world_visibility.h"
#include "player_internal.h"

#include "raymath.h"

#include <math.h>

static DoorEvent *get_usable_event(AppState *app) {
    DoorEvent *best = NULL;
    float best_distance = 9999.0f;
    int i;
    Vector2 facing = bt3d_player_forward_xz(app);
    Vector2 player_pos = { app->player.position.x, app->player.position.z };

    for (i = 0; i < app->entities.event_count; ++i) {
        DoorEvent *event = &app->entities.events[i];
        Vector2 delta;
        float distance;
        float dot;

        if (!bt3d_door_event_is_player_usable(event->base.type)) continue;

        delta = Vector2Subtract(bt3d_door_center(event), player_pos);
        distance = Vector2Length(delta);
        if (distance > PLAYER_USE_DISTANCE || distance < 0.0001f) continue;
        dot = Vector2DotProduct(Vector2Scale(delta, 1.0f / distance), facing);
        if (dot < 0.45f) continue;
        if (distance < best_distance) {
            best_distance = distance;
            best = event;
        }
    }
    return best;
}

static int get_looked_at_wall(AppState *app, int *out_x, int *out_y) {
    float distance = 0.0f;
    Vector3 direction = bt3d_player_forward(app);
    while (distance <= 1.6f) {
        Vector3 sample = Vector3Add(app->player.position, Vector3Scale(direction, distance));
        int cell_x = (int)floorf(sample.x);
        int cell_y = (int)floorf(sample.z);
        if (bt3d_is_visibility_blocked_cell(app, cell_x, cell_y)) {
            if (out_x) *out_x = cell_x;
            if (out_y) *out_y = cell_y;
            return 1;
        }
        distance += 0.05f;
    }
    return 0;
}

void bt3d_player_try_use(AppState *app) {
    DoorEvent *event = get_usable_event(app);
    int cell_x = -1;
    int cell_y = -1;

    if (event) {
        int required_key = bt3d_required_key_slot_for_event_type(event->base.type);
        if (required_key > 0 && !app->player.keys[required_key - 1]) {
            play_sound_id(app, BT3D_SOUND_UI_BACK, 0.8f);
        } else {
            bt3d_request_door_open(app, event);
        }
        return;
    }

    if (get_looked_at_wall(app, &cell_x, &cell_y) && bt3d_consume_exit_tile(app, cell_x, cell_y)) {
        play_sound_id(app, BT3D_SOUND_LEVEL_EXIT, 0.8f);
        bt3d_begin_level_transition(app, app->session.current_map_index + 1);
    }
}

void bt3d_player_trigger_remote_descriptor_events(AppState *app) {
    int player_cell_x = (int)floorf(app->player.position.x);
    int player_cell_y = (int)floorf(app->player.position.z);
    int offset_y;
    int offset_x;

    for (offset_y = -1; offset_y <= 1; ++offset_y) {
        for (offset_x = -1; offset_x <= 1; ++offset_x) {
            int cell_x = player_cell_x + offset_x;
            int cell_y = player_cell_y + offset_y;
            int index;
            uint16_t tile;
            int low_byte;
            const uint8_t *descriptor;
            DoorEvent *event;

            if (!bt3d_cell_in_bounds(cell_x, cell_y)) continue;
            index = bt3d_tile_index_for(cell_x, cell_y);
            tile = app->content.map.tile_grid[index];
            low_byte = tile & 0xff;
            if (low_byte <= 0) continue;
            if (!bt3d_is_remote_trigger_tile(tile)) continue;

            /* Kind 1 holds open the door at the descriptor's cell. */
            descriptor = &app->content.map.descriptors[(low_byte - 1) * 3];
            if (descriptor[0] != 1) continue;

            event = bt3d_find_event_by_cell(app, descriptor[1], descriptor[2]);
            if (event && event->base.state == DOOR_STATE_CLOSED) {
                bt3d_request_door_hold_open(app, event);
            }
            app->content.map.tile_grid[index] = (tile & 0xefff) & 0xff00;
        }
    }
}

void bt3d_player_collect_markers(AppState *app) {
    int i;
    Vector2 player_pos = { app->player.position.x, app->player.position.z };

    for (i = 0; i < app->entities.marker_count; ++i) {
        MarkerRuntime *marker = &app->entities.markers[i];
        if (marker->collected) continue;
        if (Vector2DistanceSqr(player_pos, marker_world_pos(marker)) > PICKUP_DISTANCE * PICKUP_DISTANCE) continue;
        marker->collected = bt3d_apply_pickup(app, marker->base.type);
    }
}
