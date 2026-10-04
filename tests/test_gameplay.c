#include "bt3d_app_state.h"
#include "bt3d_platform.h"
#include "bt3d_save_internal.h"
#include "bt3d_savegame.h"
#include "bt3d_session.h"
#include "player_internal.h"

#include <math.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #condition); \
        return 0; \
    } \
} while (0)

static AppState app;
static AppState before;
static char save_path[] = "/tmp/bt3d_gameplay_XXXXXX";

static int setup(void) {
    /* Empty floor grid, one marker and one door; a second entry is truncated. */
    const size_t records = 13 + 8 + 0x24 + 8 + BT3D_GRID_CELLS * 6;
    const size_t size = records + 4 + 6 + 255 * 3;
    unsigned char *bytes;

    bt3d_reset_app_state(&app);
    app.content.pack.data = calloc(size, 1);
    app.content.pack.entries = calloc(2, sizeof(DatPackEntry));
    app.session.map_entries = calloc(2, sizeof(DatPackEntry *));
    CHECK(app.content.pack.data && app.content.pack.entries && app.session.map_entries);
    app.content.pack.size = size;
    app.content.pack.entry_count = 2;
    app.session.map_entry_count = 2;
    app.content.pack.entries[0] = (DatPackEntry){ "MAP_TEST", 0, (uint32_t)size };
    app.content.pack.entries[1] = (DatPackEntry){ "MAP_BAD", 0, 1 };
    app.session.map_entries[0] = &app.content.pack.entries[0];
    app.session.map_entries[1] = &app.content.pack.entries[1];
    bytes = app.content.pack.data;
    bytes[13] = 1; /* marker count */
    bytes[17] = 1; /* event count */
    bytes[records] = 1;
    bytes[records + 1] = 1;
    bytes[records + 2] = 1;
    bytes[records + 5] = 2;
    bytes[records + 6] = 2;
    CHECK(start_new_game(&app));
    app.player.health = 42;
    app.entities.events[0].openness = 0.5f;
    app.entities.markers[0].collected = 1;
    app.entities.enemies = calloc(1, sizeof(EnemyRuntime));
    CHECK(app.entities.enemies);
    app.entities.enemy_count = 1;
    app.entities.enemies[0].health = 13;
    app.projectiles.player_count = 1;
    app.projectiles.player[0].life = 0.75f;
    app.projectiles.enemy_count = 1;
    app.projectiles.enemy[0].life = 0.5f;
    app.map_state.discovered_tile_mask[7] = 1;
    return 1;
}

static void cleanup(void) {
    bt3d_free_entities(&app);
    map242_unload(&app.content.map);
    datpack_unload(&app.content.pack);
    free(app.session.map_entries);
}

static int test_timers_and_round_trip(void) {
    app.player.shot_cooldown = 0.01f;
    app.player.attack_timer = 0.005f;
    app.transition.damage_flash_timer = 0.01f;
    bt3d_player_update_timers(&app, 0.02f);
    CHECK(app.player.shot_cooldown == 0.0f);
    CHECK(app.player.attack_timer == 0.0f);
    CHECK(app.transition.damage_flash_timer == 0.0f);
    bt3d_player_update_timers(&app, 0.02f);
    CHECK(app.player.shot_cooldown == 0.0f && app.player.attack_timer == 0.0f);
    CHECK(bt3d_save_game_to_file(&app, save_path));
    app.player.health = 1;
    app.entities.events[0].openness = 0.0f;
    app.entities.markers[0].collected = 0;
    app.entities.enemies[0].health = 1;
    app.projectiles.player_count = app.projectiles.enemy_count = 0;
    app.map_state.discovered_tile_mask[7] = 0;
    CHECK(load_game_from_file(&app, save_path));
    CHECK(app.player.health == 42);
    CHECK(app.entities.event_count == 1 && app.entities.events[0].openness == 0.5f);
    CHECK(app.entities.marker_count == 1 && app.entities.markers[0].collected == 1);
    CHECK(app.entities.enemy_count == 1 && app.entities.enemies[0].health == 13);
    CHECK(app.projectiles.player_count == 1 && app.projectiles.player[0].life == 0.75f);
    CHECK(app.projectiles.enemy_count == 1 && app.projectiles.enemy[0].life == 0.5f);
    CHECK(app.map_state.discovered_tile_mask[7] == 1);
    CHECK(app.map_state.event_index_by_cell[bt3d_tile_index_for(2, 2)] == 0);
    CHECK(!app.map_state.visibility_valid && app.map_state.chase_target_cell == -1);
    return 1;
}

static int replace_header(const SaveHeader *header) {
    FILE *file = fopen(save_path, "r+b");
    int ok;
    CHECK(file);
    ok = fwrite(header, sizeof(*header), 1, file) == 1;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int test_legacy_timers(void) {
    SaveHeader header;
    CHECK(bt3d_save_game_to_file(&app, save_path));
    CHECK(bt3d_read_save_header(save_path, &header));
    header.player.shot_cooldown = -0.01f;
    header.player.attack_timer = -0.02f;
    CHECK(replace_header(&header));
    CHECK(bt3d_read_save_header(save_path, &header));
    CHECK(header.player.shot_cooldown == 0.0f && header.player.attack_timer == 0.0f);
    CHECK(load_game_from_file(&app, save_path));
    CHECK(app.player.shot_cooldown == 0.0f && app.player.attack_timer == 0.0f);
    return 1;
}

static int test_failed_load_preserves_game(void) {
    SaveHeader header;
    size_t lengths[] = {
        sizeof(SaveHeader) - 1,
        sizeof(SaveHeader) + sizeof(DoorEvent) - 1,
        sizeof(SaveHeader) + sizeof(DoorEvent) + sizeof(MarkerRuntime) - 1,
        sizeof(SaveHeader) + sizeof(DoorEvent) + sizeof(MarkerRuntime) + sizeof(EnemyRuntime) - 1,
        sizeof(SaveHeader) + sizeof(DoorEvent) + sizeof(MarkerRuntime) + sizeof(EnemyRuntime) + sizeof(PlayerProjectile) - 1,
        sizeof(SaveHeader) + sizeof(DoorEvent) + sizeof(MarkerRuntime) + sizeof(EnemyRuntime) + sizeof(PlayerProjectile) + sizeof(EnemyProjectile) - 1
    };
    size_t i;

    before = app;
    for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        CHECK(bt3d_save_game_to_file(&app, save_path));
        CHECK(truncate(save_path, (off_t)lengths[i]) == 0);
        CHECK(!load_game_from_file(&app, save_path));
        CHECK(memcmp(&app, &before, sizeof(app)) == 0);
        CHECK(app.entities.enemies[0].health == 13);
        CHECK(app.content.map.events[0].x == 2);
    }
    CHECK(bt3d_save_game_to_file(&app, save_path));
    CHECK(bt3d_read_save_header(save_path, &header));
    header.map_index = 1; /* Valid campaign index, but map parsing fails. */
    CHECK(replace_header(&header));
    CHECK(!load_game_from_file(&app, save_path));
    CHECK(memcmp(&app, &before, sizeof(app)) == 0);
    header.map_index = 99;
    CHECK(replace_header(&header));
    CHECK(!load_game_from_file(&app, save_path));
    CHECK(memcmp(&app, &before, sizeof(app)) == 0);
    header.map_index = 0;
    header.player.attack_timer = NAN;
    CHECK(replace_header(&header));
    CHECK(!load_game_from_file(&app, save_path));
    CHECK(memcmp(&app, &before, sizeof(app)) == 0);
    return 1;
}

static int test_atomic_save(void) {
    char temporary_path[128];
    char directory_path[128];
    SaveHeader header;
    struct stat original;
    struct stat after;
    pid_t child;
    int status;
    FILE *file;

    CHECK(bt3d_save_game_to_file(&app, save_path));
    CHECK(stat(save_path, &original) == 0);
    snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", save_path);
    app.player.health = 77;

    /* Failure to open the temporary file must not touch the old save. */
    CHECK(mkdir(temporary_path, 0700) == 0);
    CHECK(!bt3d_save_game_to_file(&app, save_path));
    CHECK(rmdir(temporary_path) == 0);
    CHECK(bt3d_read_save_header(save_path, &header) && header.player.health == 42);

    /* A child with a small file limit forces a real write/close failure. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        struct rlimit limit = { 512, 512 };
        if (signal(SIGXFSZ, SIG_IGN) == SIG_ERR || setrlimit(RLIMIT_FSIZE, &limit) != 0) _exit(2);
        _exit(bt3d_save_game_to_file(&app, save_path) ? 1 : 0);
    }
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(access(temporary_path, F_OK) != 0);
    CHECK(stat(save_path, &after) == 0 && after.st_ino == original.st_ino);
    CHECK(load_game_from_file(&app, save_path) && app.player.health == 42);

    /* Replacement failure cleans up the temporary file and keeps the target. */
    snprintf(directory_path, sizeof(directory_path), "%s.dir", save_path);
    CHECK(mkdir(directory_path, 0700) == 0);
    CHECK(!bt3d_save_game_to_file(&app, directory_path));
    CHECK(stat(directory_path, &after) == 0 && S_ISDIR(after.st_mode));
    snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", directory_path);
    CHECK(access(temporary_path, F_OK) != 0);
    CHECK(rmdir(directory_path) == 0);

    /* An interrupted attempt must not prevent the next successful save. */
    snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", save_path);
    file = fopen(temporary_path, "wb");
    CHECK(file);
    CHECK(fputs("unfinished save", file) >= 0);
    CHECK(fclose(file) == 0);
    app.player.health = 77;
    CHECK(bt3d_save_game_to_file(&app, save_path));
    CHECK(access(temporary_path, F_OK) != 0);
    CHECK(load_game_from_file(&app, save_path) && app.player.health == 77);
    app.player.health = 42;
    return 1;
}

static int test_transitions(void) {
    app.player.lives = 2;
    app.player.health = 0;
    app.player.keys[0] = 1;
    app.player.weapon_slots[4] = 1;
    app.player.current_weapon_slot = 4;
    app.transition.death_active = 1;
    app.transition.death_timer = 0.01f;
    CHECK(bt3d_player_update_transitions(&app, 0.02f));
    CHECK(!app.transition.death_active);
    CHECK(app.player.health == 99 && app.player.lives == 2);
    CHECK(app.player.keys[0] == 0 && app.player.weapon_slots[4] == 0);
    CHECK(app.player.current_weapon_slot == 2);

    app.player.lives = 0;
    app.transition.death_active = 1;
    app.transition.death_timer = 0.01f;
    CHECK(bt3d_player_update_transitions(&app, 0.02f));
    CHECK(app.player.health == 99 && app.player.lives == 3);
    CHECK(!app.transition.death_active && app.session.current_map_index == 0);

    app.player.keys[1] = 1;
    app.transition.active = 1;
    app.transition.timer = 0.01f;
    app.transition.next_index = 0;
    CHECK(bt3d_player_update_transitions(&app, 0.02f));
    CHECK(!app.transition.active && app.player.keys[1] == 0);
    CHECK(load_game_from_file(&app, "savegame.dat"));
    CHECK(app.player.health == 99 && app.player.keys[1] == 0);
    CHECK(unlink("savegame.dat") == 0);
    return 1;
}

static int test_map_change(void) {
    before = app;
    CHECK(!bt3d_start_map(&app, 1));
    CHECK(memcmp(&app, &before, sizeof(app)) == 0);
    CHECK(app.entities.events[0].openness == 0.5f);
    CHECK(bt3d_start_map(&app, 0));
    CHECK(app.projectiles.player_count == 0 && app.projectiles.enemy_count == 0);
    CHECK(app.entities.markers[0].collected == 0);
    CHECK(app.entities.events[0].openness == 0.0f);
    CHECK(app.player.health == 42);
    return 1;
}

int main(void) {
    char working_directory[] = "/tmp/bt3d_gameplay_dir_XXXXXX";
    int fd = mkstemp(save_path);
    int ok;
    if (fd < 0) return 1;
    close(fd);
    if (!mkdtemp(working_directory) || chdir(working_directory) != 0) {
        unlink(save_path);
        return 1;
    }
    ok = setup()
        && test_timers_and_round_trip()
        && test_legacy_timers()
        && test_failed_load_preserves_game()
        && test_atomic_save()
        && test_map_change()
        && test_transitions();
    cleanup();
    unlink(save_path);
    unlink("savegame.dat");
    chdir("/tmp");
    rmdir(working_directory);
    return ok ? 0 : 1;
}
