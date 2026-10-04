#include "bt3d_app_state.h"
#include "bt3d_math.h"
#include "bt3d_world_overlays.h"

/* Animated VEC_ overlays: the base id seen in map data cycles through `frames`
   every OVERLAY_ANIMATION_STEP, or with a 10% chance per step when random. */
typedef struct {
    int base_id;
    int random_step;
    const int *frames;
    int frame_count;
} OverlayAnimation;

static const int vec_frames_5[] = { 5, 5, 36, 36, 37, 37, 36, 36 };
static const int vec_frames_8[] = { 8, 9 };
static const int vec_frames_11[] = { 11, 12 };
static const int vec_frames_13[] = { 13, 13, 14, 14, 15, 15, 14, 14 };
static const int vec_frames_16[] = { 16, 17, 18, 17 };
static const int vec_frames_24[] = { 24, 24, 24, 59, 59, 59 };
static const int vec_frames_28[] = { 28, 29 };
static const int vec_frames_30[] = { 30, 31, 32, 31, 30, 33, 34, 33 };
static const int vec_frames_44[] = { 44, 44, 54, 54 };
static const int vec_frames_47[] = { 47, 47, 55, 55, 56, 56, 55, 55 };
static const int vec_frames_86[] = { 86, 87, 88, 89 };
static const int vec_frames_95[] = { 95, 96 };
static const int vec_frames_97[] = { 97, 98 };
static const int vec_frames_103[] = { 103, 104 };
static const int vec_frames_105[] = { 105, 106 };
static const int vec_frames_160[] = { 160, 161, 162, 161, 162, 161, 160, 163, 164, 163, 164, 163 };

#define ANIMATION(base_id, random_step, frames) { base_id, random_step, frames, ARRAY_COUNT(frames) }

static const OverlayAnimation animations[] = {
    ANIMATION(5, 0, vec_frames_5),
    ANIMATION(8, 1, vec_frames_8),
    ANIMATION(11, 1, vec_frames_11),
    ANIMATION(13, 0, vec_frames_13),
    ANIMATION(16, 0, vec_frames_16),
    ANIMATION(24, 0, vec_frames_24),
    ANIMATION(28, 0, vec_frames_28),
    ANIMATION(30, 1, vec_frames_30),
    ANIMATION(44, 0, vec_frames_44),
    ANIMATION(47, 0, vec_frames_47),
    ANIMATION(86, 0, vec_frames_86),
    ANIMATION(95, 1, vec_frames_95),
    ANIMATION(97, 1, vec_frames_97),
    ANIMATION(103, 0, vec_frames_103),
    ANIMATION(105, 0, vec_frames_105),
    ANIMATION(160, 0, vec_frames_160),
};

#undef ANIMATION

_Static_assert(ARRAY_COUNT(animations) == BT3D_OVERLAY_ANIMATION_COUNT, "one cursor per animation in MapRuntimeState");

void bt3d_update_overlay_animations(AppState *app, float dt) {
    MapRuntimeState *map_state = &app->map_state;
    int i;

    map_state->overlay_animation_clock += dt;
    while (map_state->overlay_animation_clock >= OVERLAY_ANIMATION_STEP) {
        map_state->overlay_animation_clock -= OVERLAY_ANIMATION_STEP;
        for (i = 0; i < ARRAY_COUNT(animations); ++i) {
            if (!animations[i].random_step || GetRandomValue(0, 100) < 10) {
                unsigned char *cursor = &map_state->overlay_animation_cursors[i];
                *cursor = (unsigned char)((*cursor + 1) % animations[i].frame_count);
            }
        }
    }
}

int bt3d_animated_overlay_id(const AppState *app, int base_id) {
    int i;
    for (i = 0; i < ARRAY_COUNT(animations); ++i) {
        if (animations[i].base_id == base_id) return animations[i].frames[app->map_state.overlay_animation_cursors[i]];
    }
    return base_id;
}
