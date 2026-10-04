#include "bt3d_app_state.h"
#include "bt3d_enemy.h"
#include "bt3d_platform.h"
#include "bt3d_save_internal.h"
#include "bt3d_save_slots.h"
#include "bt3d_savegame.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int save_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");

    if (!file) return 0;
    fclose(file);
    return 1;
}

static int compare_save_entries_desc(const void *lhs, const void *rhs) {
    const SaveEntry *a = (const SaveEntry *)lhs;
    const SaveEntry *b = (const SaveEntry *)rhs;

    if (a->mod_time > b->mod_time) return -1;
    if (a->mod_time < b->mod_time) return 1;
    return strcmp(a->path, b->path);
}

static void format_local_time(time_t when, char *out, size_t out_size) {
    struct tm *tm_info = localtime(&when);
    if (tm_info) {
        strftime(out, out_size, "%Y-%m-%d %H:%M", tm_info);
    } else {
        snprintf(out, out_size, "unknown");
    }
}

/* Saves from other versions of the game are left out. */
static void add_save_entry(AppState *app, const char *path) {
    struct stat st;
    SaveHeader header;
    SaveEntry *entry;
    char time_buf[32];

    if (app->saves.count >= MAX_SAVE_ENTRIES || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;
    if (!bt3d_read_save_header(path, &header)) return;

    entry = &app->saves.entries[app->saves.count++];
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    format_local_time(st.st_mtime, time_buf, sizeof(time_buf));
    snprintf(entry->label, sizeof(entry->label), "Level %d [%s] - %s", header.map_index + 1, bt3d_difficulty_name(header.difficulty), time_buf);
    entry->mod_time = st.st_mtime;
}

static int build_named_save_path(const char *name, char *out_path, size_t out_path_size) {
    char cleaned[96];
    char file_name[128];
    size_t write_index = 0;
    size_t i;

    for (i = 0; name[i] != '\0' && write_index + 1 < sizeof(cleaned); ++i) {
        unsigned char ch = (unsigned char)name[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            cleaned[write_index++] = (char)ch;
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            cleaned[write_index++] = '-';
        }
    }
    while (write_index > 0 && cleaned[write_index - 1] == '-') {
        write_index--;
    }
    cleaned[write_index] = '\0';
    if (write_index == 0) return 0;

    snprintf(file_name, sizeof(file_name), "save-%s.dat", cleaned);
    return bt3d_build_save_path(file_name, out_path, out_path_size);
}

int bt3d_save_game_to_latest_slot(AppState *app) {
    char path[256];
    return bt3d_build_quicksave_path(path, sizeof(path)) && bt3d_save_game_to_file(app, path);
}

int bt3d_load_game_from_latest_slot(AppState *app) {
    char path[256];
    return bt3d_build_quicksave_path(path, sizeof(path)) && load_game_from_file(app, path);
}

/* The quicksave first, then every save-*.dat, newest first. */
void refresh_save_entries(AppState *app) {
    DIR *dir;
    struct dirent *entry;
    char path[256];

    app->saves.count = 0;
    if (bt3d_build_quicksave_path(path, sizeof(path))) add_save_entry(app, path);

    dir = opendir(bt3d_save_directory());
    if (dir) {
        while ((entry = readdir(dir)) != NULL && app->saves.count < MAX_SAVE_ENTRIES) {
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len < 10 || strncmp(name, "save-", 5) != 0 || strcmp(name + len - 4, ".dat") != 0) continue;
            if (bt3d_build_save_path(name, path, sizeof(path))) add_save_entry(app, path);
        }
        closedir(dir);
    }
    if (app->saves.count > 1) {
        qsort(app->saves.entries, (size_t)app->saves.count, sizeof(SaveEntry), compare_save_entries_desc);
    }
}

void default_save_name_with_level(AppState *app, char *out_name, size_t out_name_size) {
    char date_buf[32];

    format_local_time(time(NULL), date_buf, sizeof(date_buf));
    snprintf(out_name, out_name_size, "Level %d - %s", app->session.current_map_index + 1, date_buf);
}

/* Saves under a file named after `name`, numbered when taken, and as the quicksave. */
int save_game_to_named_slot(AppState *app, const char *name) {
    char candidate[256];
    char base_path[256];
    int suffix = 2;

    if (!build_named_save_path(name, base_path, sizeof(base_path))) return 0;
    snprintf(candidate, sizeof(candidate), "%s", base_path);
    while (save_file_exists(candidate)) {
        snprintf(candidate, sizeof(candidate), "%.*s-%d.dat", (int)(strlen(base_path) - 4), base_path, suffix++);
    }
    if (!bt3d_save_game_to_file(app, candidate)) return 0;
    bt3d_save_game_to_latest_slot(app);
    return 1;
}

int delete_save_file(AppState *app, const char *path) {
    if (remove(path) != 0) return 0;
    bt3d_sync_persistent_storage();
    refresh_save_entries(app);
    return 1;
}
