#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_campaign.h"
#include "bt3d_enemy.h"
#include "bt3d_platform.h"
#include "bt3d_session.h"
#include "bt3d_world_classify.h"
#include "bt3d_world_markers.h"
#include "bt3d_world_spawn.h"

#include <stdlib.h>
#include <string.h>

void bt3d_free_entities(AppState *app) {
    free(app->entities.events);
    free(app->entities.markers);
    free(app->entities.enemies);
    memset(&app->entities, 0, sizeof(app->entities));
}

/* Everything derived from the map itself: which cell holds which door, the
   decorations hanging from the ceiling, and the per-cell caches. */
static void build_map_state(AppState *app) {
    const Map242 *map = &app->content.map;
    size_t i;

    memset(&app->map_state, 0, sizeof(app->map_state));
    memset(app->map_state.event_index_by_cell, -1, sizeof(app->map_state.event_index_by_cell));
    app->map_state.chase_target_cell = -1;
    for (i = 0; i < map->event_count; ++i) {
        if (bt3d_cell_in_bounds(map->events[i].x, map->events[i].y)) {
            app->map_state.event_index_by_cell[bt3d_tile_index_for(map->events[i].x, map->events[i].y)] = (short)i;
        }
    }
    for (i = 0; i < map->marker_count; ++i) {
        const MapMarker *marker = &map->markers[i];
        if (marker_hangs_from_ceiling(app, marker->type) && bt3d_cell_in_bounds(marker->cell_x, marker->cell_y)) {
            app->map_state.ceiling_marker_cell_mask[bt3d_tile_index_for(marker->cell_x, marker->cell_y)] = 1;
        }
    }
    bt3d_build_static_cell_caches(app);
}

int bt3d_load_map(AppState *app, int index) {
    const DatPackEntry *entry;
    Map242 map = { 0 };

    if (index < 0 || index >= app->session.map_entry_count) return 0;
    entry = app->session.map_entries[index];
    if (!map242_parse(&map, entry->name, datpack_entry_data(&app->content.pack, entry), entry->length)) return 0;
    bt3d_free_entities(app);
    map242_unload(&app->content.map);
    app->content.map = map;
    app->session.current_map_index = index;
    build_map_state(app);

    /* Nothing in flight carries over to the new map. */
    app->projectiles.player_count = 0;
    app->projectiles.enemy_count = 0;
    app->projectiles.shot_impact_count = 0;
    memset(&app->transition, 0, sizeof(app->transition));
    app->control.map_overlay_active = 0;
    app->audio.ambient_timer = 45.0f + (float)GetRandomValue(0, 15);
    app->audio.last_ambient_sound = 0;
    return 1;
}

/* Doors, pickups and enemies as the map places them. */
static void spawn_entities(AppState *app) {
    const Map242 *map = &app->content.map;
    size_t i;

    if (map->event_count > 0 && (app->entities.events = (DoorEvent *)calloc(map->event_count, sizeof(DoorEvent)))) {
        app->entities.event_count = (int)map->event_count;
        for (i = 0; i < map->event_count; ++i) {
            app->entities.events[i].base = map->events[i];
            app->entities.events[i].openness = map->events[i].state != 0 ? 1.0f : 0.0f;
        }
    }
    if (map->marker_count > 0 && (app->entities.markers = (MarkerRuntime *)calloc(map->marker_count, sizeof(MarkerRuntime)))) {
        app->entities.marker_count = (int)map->marker_count;
        for (i = 0; i < map->marker_count; ++i) app->entities.markers[i].base = map->markers[i];
    }
    if (map->object_count > 0 && (app->entities.enemies = (EnemyRuntime *)calloc(map->object_count, sizeof(EnemyRuntime)))) {
        for (i = 0; i < map->object_count; ++i) {
            const MapObject *object = &map->objects[i];
            if (object->class_id >= BT3D_ENEMY_CLASS_COUNT || map242_is_player_start_object(object)) continue;
            bt3d_init_enemy_runtime(app, &app->entities.enemies[app->entities.enemy_count++], object, (int)i);
        }
    }
}

int bt3d_start_map(AppState *app, int index) {
    if (!bt3d_load_map(app, index)) return 0;
    spawn_entities(app);
    place_player_at_spawn(app);
    return 1;
}

int start_new_game(AppState *app) {
    init_player_state(app);
    if (!bt3d_start_map(app, 0)) return 0;
    app->session.in_session = 1;
    return 1;
}

int bt3d_load_initial_pack(AppState *app) {
    if (!bt3d_open_data_pack(&app->content.pack)) return 0;
    bt3d_load_assets(app);
    return bt3d_build_campaign_map_entry_list(app);
}

static int wrap_map_index(const AppState *app, int index) {
    if (index < 0) return app->session.map_entry_count - 1;
    if (index >= app->session.map_entry_count) return 0;
    return index;
}

void bt3d_cycle_map(AppState *app, int delta) {
    if (app->session.map_entry_count <= 0) return;
    bt3d_start_map(app, wrap_map_index(app, app->session.current_map_index + delta));
}

void bt3d_begin_level_transition(AppState *app, int next_index) {
    if (app->session.map_entry_count <= 0) return;
    app->transition.active = 1;
    app->transition.timer = 1.4f;
    app->transition.next_index = wrap_map_index(app, next_index);
}
