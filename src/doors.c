#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_doors.h"
#include "raymath.h"

#include <math.h>

enum {
    DOOR_SOUND_KEYED = 28,
    DOOR_SOUND_SECRET = 26,
    DOOR_SOUND_NORMAL = 25,
    DOOR_SOUND_CLOSE = 23
};

static int sound_id_for_door_open_event(int event_type) {
    if (event_type >= DOOR_KEY_RED_EVENT && event_type <= DOOR_KEY_BLUE_EVENT) return DOOR_SOUND_KEYED;
    if (event_type == DOOR_SECRET_OPEN_EVENT) return DOOR_SOUND_SECRET;
    return DOOR_SOUND_NORMAL;
}

DoorEvent *bt3d_find_event_by_cell(const AppState *app, int x, int y) {
    int idx;
    if (!bt3d_cell_in_bounds(x, y)) return NULL;
    idx = app->map_state.event_index_by_cell[bt3d_tile_index_for(x, y)];
    if (idx < 0 || idx >= app->entities.event_count) return NULL;
    return &app->entities.events[idx];
}

/* Key doors use the key slot matching their event type. */
int bt3d_required_key_slot_for_event_type(int event_type) {
    return (event_type >= DOOR_KEY_RED_EVENT && event_type <= DOOR_KEY_BLUE_EVENT) ? event_type : 0;
}

int bt3d_door_event_is_player_usable(int event_type) {
    return event_type == DOOR_PLAIN_EVENT
        || bt3d_required_key_slot_for_event_type(event_type) > 0
        || event_type == DOOR_SECRET_OPEN_EVENT;
}

int bt3d_is_center_split_door_tile(uint16_t tile) {
    /* Mask out the 0x0100 bit (keyed variant) so 0xb100 / 0xb500 are still
       recognised as split doors like their 0xb000 / 0xb400 base families.
       0xf400 / 0xf500 are also split families (observed on MAP_18 38,27). */
    uint16_t family = tile & 0xfe00;
    return family == 0xb000 || family == 0xb400 || family == 0xf400;
}

int bt3d_split_door_slice_index(uint16_t tile, int slides_on_x_axis, float leaf_offset) {
    /* Slice 0 is the left half of the split texture, slice 1 the right.
       Leaf_offset < 0 is the near/left leaf, > 0 is the far/right leaf.
       The only case where the viewer sees the two halves swapped is a
       0x40 z-slide door (front faces +X), so that pair gets the flip.
       Every other combination (0x40 x-slide, 0xc0 either axis) keeps the
       natural leaf_offset -> slice mapping. Confirmed against sample doors
       in MAP_3, MAP_6, MAP_7, MAP_17 and MAP_18. */
    int swap = ((tile & 0xff) == 0x40) && !slides_on_x_axis;
    if (swap) return leaf_offset < 0.0f ? 1 : 0;
    return leaf_offset < 0.0f ? 0 : 1;
}

void bt3d_request_door_open(AppState *app, DoorEvent *event) {
    if (!event) return;
    event->hold_open = 0;
    if (event->openness >= BT3D_DOOR_OPEN_THRESHOLD) {
        event->base.state = DOOR_STATE_CLOSING;
        event->wait = 0.0f;
        return;
    }
    if (event->base.state == DOOR_STATE_OPENING) {
        event->wait = 0.0f;
        return;
    }
    event->base.state = DOOR_STATE_OPENING;
    event->wait = 0.0f;
    play_sound_id(app, sound_id_for_door_open_event(event->base.type), 0.8f);
}

/* Like bt3d_request_door_open, but the door stays open instead of closing
   again after its wait. */
void bt3d_request_door_hold_open(AppState *app, DoorEvent *event) {
    if (!event) return;
    if (event->openness >= BT3D_DOOR_OPEN_THRESHOLD) {
        event->base.state = DOOR_STATE_HELD_OPEN;
        event->wait = 0.0f;
    } else {
        bt3d_request_door_open(app, event);
    }
    event->hold_open = 1;
}

static int doorway_is_occupied(AppState *app, const DoorEvent *event) {
    Vector2 center;
    Vector2 player_delta;
    int i = 0;

    center = bt3d_door_center(event);
    player_delta = Vector2Subtract((Vector2){ app->player.position.x, app->player.position.z }, center);
    if (fabsf(player_delta.x) <= 0.52f && fabsf(player_delta.y) <= 0.52f) {
        return 1;
    }

    for (i = 0; i < app->entities.enemy_count; ++i) {
        EnemyRuntime *enemy = &app->entities.enemies[i];
        Vector2 enemy_delta;
        if (!enemy->alive && enemy->anim_state != ENEMY_ANIM_DIE) continue;
        enemy_delta = Vector2Subtract((Vector2){ enemy->position.x, enemy->position.z }, center);
        if (fabsf(enemy_delta.x) <= 0.52f && fabsf(enemy_delta.y) <= 0.52f) {
            return 1;
        }
    }

    return 0;
}

static void update_event(AppState *app, DoorEvent *event, float dt) {
    float speed = (event->base.type == DOOR_PLAIN_EVENT || event->base.type == DOOR_FAST_OPEN_EVENT) ? EVENT_SPEED_FAST : EVENT_SPEED_NORMAL;

    if (event->base.state == DOOR_STATE_OPENING) {
        event->openness += speed * dt;
        if (event->openness >= 1.0f) {
            event->openness = 1.0f;
            if (event->hold_open) {
                event->base.state = DOOR_STATE_HELD_OPEN;
                event->wait = 0.0f;
            } else {
                event->base.state = DOOR_STATE_WAITING;
                event->wait = 1.5f;
            }
        }
    } else if (event->base.state == DOOR_STATE_CLOSING) {
        if (doorway_is_occupied(app, event)) {
            event->base.state = DOOR_STATE_OPENING;
            event->wait = 0.0f;
            return;
        }
        event->openness -= speed * dt;
        if (event->openness <= 0.0f) {
            event->openness = 0.0f;
            event->base.state = DOOR_STATE_CLOSED;
            play_sound_id(app, DOOR_SOUND_CLOSE, 0.7f);
        }
    } else if (event->base.state == DOOR_STATE_WAITING) {
        if (doorway_is_occupied(app, event)) {
            event->wait = 0.15f;
            return;
        }
        event->wait -= dt;
        if (event->wait <= 0.0f) {
            event->wait = 0.0f;
            event->base.state = DOOR_STATE_CLOSING;
        }
    } else if (event->base.state == DOOR_STATE_HELD_OPEN) {
        event->openness = 1.0f;
    }
}

void bt3d_update_events(AppState *app, float dt) {
    int i;
    for (i = 0; i < app->entities.event_count; ++i) {
        DoorEvent *event = &app->entities.events[i];
        int was_blocking = event->openness <= 0.0f;
        update_event(app, event, dt);
        if ((event->openness <= 0.0f) != was_blocking) app->map_state.doors_version++;
    }
}
