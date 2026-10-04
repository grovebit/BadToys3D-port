#ifndef BT3D_APP_STATE_H
#define BT3D_APP_STATE_H

#include "raylib.h"
#include "bt3d_constants.h"
#include "bt3d_types.h"
#include "datpack.h"
#include "midi_player.h"
#include "map242.h"

#if BT3D_PLATFORM_SWITCH
#include <switch.h>
#endif

typedef struct {
    float mouse_sensitivity;
    float music_volume;
    float sound_volume;
    int display_mode;
    int debug_tools_enabled;
} AppSettings;

typedef struct {
    int active;
    int screen;
    int index;
    int scroll;
    char save_name_input[64];
    int save_name_length;
    int dragging_slider;
    int quit_requested;
} MenuState;

typedef struct {
    SaveEntry entries[MAX_SAVE_ENTRIES];
    int count;
} SaveListState;

/* Full-screen effects: the level change, death and the damage flash. */
typedef struct {
    int active;
    float timer;
    int next_index;
    int death_active;
    float death_timer;
    float damage_flash_timer;
} TransitionState;

typedef struct {
    Vector3 position;
    float yaw;
    int health;
    int lives;
    int special_count;
    int keys[3];
    int weapon_slots[5];
    int weapon_ammo[5];
    int current_weapon_slot;
    float shot_cooldown;
    float attack_timer;
} PlayerState;

typedef struct {
    PlayerProjectile player[MAX_PLAYER_PROJECTILES];
    int player_count;
    EnemyProjectile enemy[MAX_ENEMY_PROJECTILES];
    int enemy_count;
    PlayerShotImpact shot_impacts[MAX_PLAYER_SHOT_IMPACTS];
    int shot_impact_count;
} ProjectileState;

typedef struct {
    DoorEvent *events;
    int event_count;
    MarkerRuntime *markers;
    int marker_count;
    EnemyRuntime *enemies;
    int enemy_count;
} EntityState;

/* Per-map state, rebuilt whenever a map loads. */
typedef struct {
    /* Index into entities.events of the door in each cell, or -1. */
    short event_index_by_cell[BT3D_GRID_CELLS];
    unsigned char ceiling_marker_cell_mask[BT3D_GRID_CELLS];
    unsigned char structural_wall_mask[BT3D_GRID_CELLS];
    unsigned char raw_floor_tex_cache[BT3D_GRID_CELLS];
    unsigned char raw_ceil_tex_cache[BT3D_GRID_CELLS];
    unsigned char discovered_tile_mask[BT3D_GRID_CELLS];
    /* Cells the player can see, and those plus their neighbours to draw. */
    unsigned char visible_tile_mask[BT3D_GRID_CELLS];
    unsigned char render_tile_mask[BT3D_GRID_CELLS];
    uint16_t render_cell_indices[BT3D_GRID_CELLS];
    int render_cell_count;
    /* Bumped whenever a door starts or stops blocking its cell. */
    unsigned doors_version;
    /* The player position and doors the masks above were built for. */
    int visibility_valid;
    Vector2 visibility_origin;
    unsigned visibility_doors_version;
    /* Steps to the player's cell for chasing enemies, -1 when unreachable;
       built for chase_target_cell (-1 when stale) and the doors as they were. */
    int16_t chase_distances[BT3D_GRID_CELLS];
    int chase_target_cell;
    unsigned chase_doors_version;
    /* Animated overlay frames, stepped on the game clock. */
    float overlay_animation_clock;
    unsigned char overlay_animation_cursors[BT3D_OVERLAY_ANIMATION_COUNT];
} MapRuntimeState;

typedef struct {
    /* One per pack entry, in pack order; texture.id is 0 for non-images. */
    PackTexture *textures;
    /* Index into textures of STN_<id> and VEC_<id>, or -1 when missing. */
    short wall_texture_entries[256];
    short vec_texture_entries[256];
    /* SND_<id>; frameCount is 0 when the pack has no such sound. */
    Sound sounds[128];
} AssetCacheState;

typedef struct {
    int initialized;
    MidiPlayer menu_midi;
    float ambient_timer;
    int last_ambient_sound;
} AudioRuntimeState;

typedef struct {
    const DatPackEntry **map_entries;
    int map_entry_count;
    int current_map_index;
    int difficulty;
    int in_session;
} GameSessionState;

typedef struct {
    int mouse_captured;
    int noclip_enabled;
    int god_mode_enabled;
    int map_overlay_active;
} ControlModeState;

/* What FrameInput is derived from, carried between frames. */
typedef struct {
#if BT3D_PLATFORM_SWITCH
    PadState pad;
#endif
    int mouse_was_down[2];
    unsigned held_directions;
    float repeat_timer;
} InputState;

/* The debug log's frame counter and the frame-time graph. */
typedef struct {
    int perf_frame;
    float frame_ms[240];
    int frame_cursor;
    int frame_count;
    float held_peak_ms;
    float held_peak_timer;
} DebugState;

typedef struct {
    DatPack pack;
    Map242 map;
} LoadedContentState;

typedef struct AppState {
    LoadedContentState content;
    GameSessionState session;
    PlayerState player;
    ProjectileState projectiles;
    EntityState entities;
    MapRuntimeState map_state;
    AssetCacheState assets;
    AudioRuntimeState audio;
    AppSettings settings;
    ControlModeState control;
    InputState input;
    DebugState debug;
    TransitionState transition;
    MenuState menu;
    SaveListState saves;
} AppState;

#endif
