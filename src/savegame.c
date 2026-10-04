#include "bt3d_app_state.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_platform.h"
#include "bt3d_save_internal.h"
#include "bt3d_savegame.h"
#include "bt3d_session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_items(FILE *file, const void *items, size_t item_size, int count) {
    return count <= 0 || fwrite(items, item_size, (size_t)count, file) == (size_t)count;
}

static int read_items(FILE *file, void *items, size_t item_size, int count) {
    return count <= 0 || fread(items, item_size, (size_t)count, file) == (size_t)count;
}

/* Reads `count` items into a new array; NULL when there are none or on failure. */
static void *read_new_items(FILE *file, size_t item_size, int count) {
    void *items = count > 0 ? malloc(item_size * (size_t)count) : NULL;
    if (items && !read_items(file, items, item_size, count)) {
        free(items);
        items = NULL;
    }
    return items;
}

static int in_range(int value, int min_value, int max_value) {
    return value >= min_value && value <= max_value;
}

static int is_valid_player(const PlayerState *player) {
    int i;

    if (!isfinite(player->position.x) || !isfinite(player->position.y) || !isfinite(player->position.z)
        || !isfinite(player->yaw)) return 0;
    if (!isfinite(player->shot_cooldown) || !isfinite(player->attack_timer)) return 0;
    if (!in_range(player->health, 0, 999) || !in_range(player->lives, 0, 99) || !in_range(player->special_count, 0, 9999)) return 0;
    if (!in_range(player->current_weapon_slot, 0, 4)) return 0;
    for (i = 0; i < ARRAY_COUNT(player->keys); ++i) {
        if (!in_range(player->keys[i], 0, 1)) return 0;
    }
    for (i = 0; i < ARRAY_COUNT(player->weapon_slots); ++i) {
        if (!in_range(player->weapon_slots[i], 0, 1) || !in_range(player->weapon_ammo[i], 0, 9999)) return 0;
    }
    return 1;
}

/* Saves are raw structs, so whatever a corrupt file could turn into a huge
   allocation, an out-of-range index or a NaN is checked. The door count is
   checked against the map once it is loaded. */
static int read_header(FILE *file, SaveHeader *header) {
    int ok = fread(header, sizeof(*header), 1, file) == 1
        && header->magic == SAVE_MAGIC
        && header->version == SAVE_FORMAT_VERSION
        && in_range(header->difficulty, BT3D_DIFFICULTY_EASY, BT3D_DIFFICULTY_HARD)
        && in_range(header->marker_count, 0, BT3D_GRID_CELLS)
        && in_range(header->enemy_count, 0, BT3D_GRID_CELLS)
        && in_range(header->player_projectile_count, 0, MAX_PLAYER_PROJECTILES)
        && in_range(header->enemy_projectile_count, 0, MAX_ENEMY_PROJECTILES)
        && is_valid_player(&header->player);
    if (ok) {
        /* Older saves can contain timers that overshot zero on their last tick. */
        header->player.shot_cooldown = fmaxf(0.0f, header->player.shot_cooldown);
        header->player.attack_timer = fmaxf(0.0f, header->player.attack_timer);
    }
    return ok;
}

int bt3d_read_save_header(const char *path, SaveHeader *header) {
    FILE *file = fopen(path, "rb");
    int ok = file && read_header(file, header);
    if (file) fclose(file);
    return ok;
}

int bt3d_save_game_to_file(AppState *app, const char *path) {
    SaveHeader header = { 0 };
    char *temporary_path;
    size_t temporary_path_size;
    FILE *file;
    int ok;

    if (!app->session.in_session) return 0;
    header.magic = SAVE_MAGIC;
    header.version = SAVE_FORMAT_VERSION;
    header.map_index = app->session.current_map_index;
    header.difficulty = app->session.difficulty;
    header.player = app->player;
    header.event_count = app->entities.event_count;
    header.marker_count = app->entities.marker_count;
    header.enemy_count = app->entities.enemy_count;
    header.player_projectile_count = app->projectiles.player_count;
    header.enemy_projectile_count = app->projectiles.enemy_count;
    memcpy(header.discovered_tile_mask, app->map_state.discovered_tile_mask, sizeof(header.discovered_tile_mask));

    /* Saves are synchronous. Reuse a sibling temporary file left by an
       interrupted write; the destination stays intact until replacement. */
    temporary_path_size = strlen(path) + sizeof(".tmp");
    temporary_path = malloc(temporary_path_size);
    if (!temporary_path) return 0;
    snprintf(temporary_path, temporary_path_size, "%s.tmp", path);
    file = fopen(temporary_path, "wb");
    if (!file) {
        free(temporary_path);
        return 0;
    }
    ok = fwrite(&header, sizeof(header), 1, file) == 1
        && write_items(file, app->entities.events, sizeof(DoorEvent), header.event_count)
        && write_items(file, app->entities.markers, sizeof(MarkerRuntime), header.marker_count)
        && write_items(file, app->entities.enemies, sizeof(EnemyRuntime), header.enemy_count)
        && write_items(file, app->projectiles.player, sizeof(PlayerProjectile), header.player_projectile_count)
        && write_items(file, app->projectiles.enemy, sizeof(EnemyProjectile), header.enemy_projectile_count);
    if (fclose(file) != 0) ok = 0;
    if (ok) ok = bt3d_replace_file(temporary_path, path);
    if (!ok) remove(temporary_path);
    free(temporary_path);
    if (ok) bt3d_sync_persistent_storage();
    return ok;
}

/* The saved doors line up with the map's own, which the per-cell door index relies on. */
static int read_entities(FILE *file, AppState *app, const SaveHeader *header) {
    if (header->event_count != (int)app->content.map.event_count) return 0;
    app->entities.events = (DoorEvent *)read_new_items(file, sizeof(DoorEvent), header->event_count);
    app->entities.markers = (MarkerRuntime *)read_new_items(file, sizeof(MarkerRuntime), header->marker_count);
    app->entities.enemies = (EnemyRuntime *)read_new_items(file, sizeof(EnemyRuntime), header->enemy_count);
    if ((header->event_count > 0 && !app->entities.events)
        || (header->marker_count > 0 && !app->entities.markers)
        || (header->enemy_count > 0 && !app->entities.enemies)) return 0;
    app->entities.event_count = header->event_count;
    app->entities.marker_count = header->marker_count;
    app->entities.enemy_count = header->enemy_count;
    app->projectiles.player_count = header->player_projectile_count;
    app->projectiles.enemy_count = header->enemy_projectile_count;
    return read_items(file, app->projectiles.player, sizeof(PlayerProjectile), header->player_projectile_count)
        && read_items(file, app->projectiles.enemy, sizeof(EnemyProjectile), header->enemy_projectile_count);
}

int load_game_from_file(AppState *app, const char *path) {
    SaveHeader header;
    FILE *file = fopen(path, "rb");
    AppState *loaded;
    int ok;

    if (!file) return 0;
    if (!read_header(file, &header)) {
        fclose(file);
        return 0;
    }
    loaded = malloc(sizeof(*loaded));
    if (!loaded) {
        fclose(file);
        return 0;
    }
    /* Borrow pack/assets and session configuration, but own the replacement
       map and entities until every read succeeds. Keep this off the stack. */
    *loaded = *app;
    memset(&loaded->content.map, 0, sizeof(loaded->content.map));
    memset(&loaded->entities, 0, sizeof(loaded->entities));
    ok = bt3d_load_map(loaded, header.map_index) && read_entities(file, loaded, &header);
    fclose(file);
    if (ok) {
        loaded->player = header.player;
        memcpy(loaded->map_state.discovered_tile_mask, header.discovered_tile_mask, sizeof(header.discovered_tile_mask));
        loaded->session.difficulty = header.difficulty;
        loaded->session.in_session = 1;
        bt3d_free_entities(app);
        map242_unload(&app->content.map);
        *app = *loaded;
    } else {
        bt3d_free_entities(loaded);
        map242_unload(&loaded->content.map);
    }
    free(loaded);
    return ok;
}
