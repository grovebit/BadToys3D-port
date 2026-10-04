#ifndef BT3D_DOORS_H
#define BT3D_DOORS_H

#include "bt3d_types.h"

typedef struct AppState AppState;

enum {
    DOOR_PLAIN_EVENT = 0,
    DOOR_KEY_RED_EVENT = 1,
    DOOR_KEY_GREEN_EVENT = 2,
    DOOR_KEY_BLUE_EVENT = 3,
    DOOR_FAST_OPEN_EVENT = 4,
    DOOR_SECRET_OPEN_EVENT = 6,

    DOOR_STATE_CLOSED = 0,
    DOOR_STATE_CLOSING = 1,
    DOOR_STATE_OPENING = 2,
    DOOR_STATE_WAITING = 3,
    DOOR_STATE_HELD_OPEN = 4
};

/* Openness at which a door counts as open; requesting it again closes it. */
#define BT3D_DOOR_OPEN_THRESHOLD 0.875f

static inline Vector2 bt3d_door_center(const DoorEvent *event) {
    return (Vector2){ event->base.x + 0.5f, event->base.y + 0.5f };
}

DoorEvent *bt3d_find_event_by_cell(const AppState *app, int x, int y);
int bt3d_required_key_slot_for_event_type(int event_type);
int bt3d_door_event_is_player_usable(int event_type);
int bt3d_is_center_split_door_tile(uint16_t tile);
int bt3d_split_door_slice_index(uint16_t tile, int slides_on_x_axis, float leaf_offset);
void bt3d_request_door_open(AppState *app, DoorEvent *event);
void bt3d_request_door_hold_open(AppState *app, DoorEvent *event);
void bt3d_update_events(AppState *app, float dt);

#endif
