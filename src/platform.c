#include "bt3d_platform.h"

#include "bt3d_app_state.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if BT3D_PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#if BT3D_PLATFORM_SWITCH
#define SAVE_DIRECTORY "sdmc:/switch/bt3d"
static const char *const data_pack_candidates[] = { "romfs:/data.pck", SAVE_DIRECTORY "/data.pck", "data.pck" };
#elif BT3D_PLATFORM_WEB
/* IDBFS, mounted and loaded by web/pre.js before main runs. */
#define SAVE_DIRECTORY "/bt3d"
/* Written by web/shell.html from the file the player picks. */
static const char *const data_pack_candidates[] = { "/data.pck" };
#else
/* The working directory, moved next to the executable at startup. */
#define SAVE_DIRECTORY "."
static const char *const data_pack_candidates[] = { "data.pck" };
#endif

#define CONFIG_MAGIC 0x42543343u  /* BT3C */
#define CONFIG_VERSION 5u
#define CONFIG_FILE_NAME "config.dat"

typedef struct {
    uint32_t magic;
    uint32_t version;
    AppSettings settings;
} ConfigFile;

void bt3d_save_config(const AppState *app) {
    ConfigFile config = { CONFIG_MAGIC, CONFIG_VERSION, app->settings };
    char path[256];
    FILE *file;

    if (!bt3d_build_save_path(CONFIG_FILE_NAME, path, sizeof(path))) return;
    file = fopen(path, "wb");
    if (!file) return;
    fwrite(&config, sizeof(config), 1, file);
    fclose(file);
    bt3d_sync_persistent_storage();
}

/* Keeps the defaults when there is no config of this version. */
void bt3d_load_config(AppState *app) {
    ConfigFile config;
    char path[256];
    FILE *file;
    int valid;

    if (!bt3d_build_save_path(CONFIG_FILE_NAME, path, sizeof(path))) return;
    file = fopen(path, "rb");
    if (!file) return;
    valid = fread(&config, sizeof(config), 1, file) == 1 && config.magic == CONFIG_MAGIC && config.version == CONFIG_VERSION;
    fclose(file);
    if (!valid) return;

    /* fmaxf/fminf also turn a NaN into a bound. */
    app->settings.mouse_sensitivity = fminf(fmaxf(config.settings.mouse_sensitivity, MOUSE_SENSITIVITY_MIN), MOUSE_SENSITIVITY_MAX);
    app->settings.music_volume = fminf(fmaxf(config.settings.music_volume, 0.0f), 1.0f);
    app->settings.sound_volume = fminf(fmaxf(config.settings.sound_volume, 0.0f), 1.0f);
    app->settings.display_mode = config.settings.display_mode == BT3D_DISPLAY_WINDOWED ? BT3D_DISPLAY_WINDOWED : BT3D_DISPLAY_BORDERLESS;
    app->settings.debug_tools_enabled = config.settings.debug_tools_enabled != 0;
}

const char *bt3d_save_directory(void) {
    return SAVE_DIRECTORY;
}

int bt3d_build_save_path(const char *file_name, char *out_path, size_t out_path_size) {
    int length = snprintf(out_path, out_path_size, SAVE_DIRECTORY "/%s", file_name);
    return length > 0 && (size_t)length < out_path_size;
}

int bt3d_build_quicksave_path(char *out_path, size_t out_path_size) {
    return bt3d_build_save_path("savegame.dat", out_path, out_path_size);
}

int bt3d_open_data_pack(DatPack *pack) {
    int i;
    for (i = 0; i < ARRAY_COUNT(data_pack_candidates); ++i) {
        if (datpack_load(pack, data_pack_candidates[i])) return 1;
    }
    return 0;
}

/* Desktop builds keep data.pck and the saves next to the executable, or
   next to the .app bundle on macOS. */
void bt3d_enter_game_directory(void) {
#if !BT3D_PLATFORM_SWITCH && !BT3D_PLATFORM_WEB && !BT3D_PLATFORM_ANDROID
    const char bundle_suffix[] = "/Contents/MacOS/";
    const char *exe_dir = GetApplicationDirectory();
    size_t length = strlen(exe_dir);

    ChangeDirectory(exe_dir);
    if (length >= sizeof(bundle_suffix) - 1 && strcmp(exe_dir + length - (sizeof(bundle_suffix) - 1), bundle_suffix) == 0) {
        ChangeDirectory("../../..");
    }
#endif
}

void bt3d_reset_app_state(AppState *app) {
    memset(app, 0, sizeof(*app));
    app->settings.mouse_sensitivity = 1.0f;
    app->settings.music_volume = 1.0f;
    app->settings.sound_volume = 1.0f;
    app->settings.display_mode = BT3D_DISPLAY_BORDERLESS;
    app->session.difficulty = BT3D_DIFFICULTY_MEDIUM;
}

static void apply_display_mode(const AppState *app) {
#if BT3D_PLATFORM_SWITCH || BT3D_PLATFORM_WEB || BT3D_PLATFORM_ANDROID
    (void)app;
#else
    int monitor;

    if (app->settings.display_mode == BT3D_DISPLAY_BORDERLESS) {
        SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        return;
    }

    ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    SetWindowSize(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);
    monitor = GetCurrentMonitor();
    if (monitor >= 0) {
        Vector2 monitor_pos = GetMonitorPosition(monitor);
        int monitor_width = GetMonitorWidth(monitor);
        int monitor_height = GetMonitorHeight(monitor);
        if (monitor_width > DEFAULT_SCREEN_WIDTH && monitor_height > DEFAULT_SCREEN_HEIGHT) {
            SetWindowPosition(
                (int)monitor_pos.x + (monitor_width - DEFAULT_SCREEN_WIDTH) / 2,
                (int)monitor_pos.y + (monitor_height - DEFAULT_SCREEN_HEIGHT) / 2);
        }
    }
#endif
}

void bt3d_toggle_display_mode(AppState *app) {
    app->settings.display_mode = app->settings.display_mode == BT3D_DISPLAY_WINDOWED ? BT3D_DISPLAY_BORDERLESS : BT3D_DISPLAY_WINDOWED;
    apply_display_mode(app);
}

/* Applies the display mode, so the config is loaded first. */
void bt3d_init_window_and_audio(AppState *app) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, "Bad Toys 3D - raylib port");
    SetExitKey(KEY_NULL);
    InitAudioDevice();
    app->audio.initialized = IsAudioDeviceReady();
    apply_display_mode(app);
#if BT3D_PLATFORM_SWITCH
    mkdir(SAVE_DIRECTORY, 0755);
#endif
}

void bt3d_sync_persistent_storage(void) {
#if BT3D_PLATFORM_WEB
    EM_ASM({
        if (typeof Module !== 'undefined' && Module.bt3dSyncPersistentStorage) {
            Module.bt3dSyncPersistentStorage();
        }
    });
#endif
}
