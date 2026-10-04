#include "bt3d_app_state.h"
#include "bt3d_debug.h"
#include "bt3d_doors.h"
#include "bt3d_input.h"
#include "bt3d_math.h"
#include "bt3d_platform.h"
#include "bt3d_player.h"
#include "bt3d_render.h"
#include "bt3d_world_classify.h"
#include "bt3d_world_doors.h"
#include "bt3d_world_grid.h"
#include "bt3d_world_markers.h"
#include "profiler.h"

#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/* Targets count within 6 units and ~15 degrees of the view direction; the
   score favours centred, close targets. */
static int look_score(Vector2 player_pos, Vector2 forward, Vector2 target, float *out_score) {
    Vector2 delta = Vector2Subtract(target, player_pos);
    float distance = Vector2Length(delta);
    float dot;

    if (distance > 6.0f || distance < 0.001f) return 0;
    dot = Vector2DotProduct(Vector2Scale(delta, 1.0f / distance), forward);
    if (dot < 0.965f) return 0;
    *out_score = dot * 4.0f - distance * 0.2f;
    return 1;
}

static MarkerRuntime *get_looked_at_marker(AppState *app) {
    MarkerRuntime *best = NULL;
    float best_score = -9999.0f;
    int i = 0;
    Vector2 forward = bt3d_player_forward_xz(app);
    Vector2 player_pos = { app->player.position.x, app->player.position.z };

    for (i = 0; i < app->entities.marker_count; ++i) {
        MarkerRuntime *marker = &app->entities.markers[i];
        float score;

        if (marker->collected) continue;
        if (!look_score(player_pos, forward, marker_world_pos(marker), &score)) continue;
        if (score > best_score) {
            best_score = score;
            best = marker;
        }
    }

    return best;
}

static DoorEvent *get_looked_at_door(AppState *app) {
    DoorEvent *best = NULL;
    float best_score = -9999.0f;
    int i = 0;
    Vector2 forward = bt3d_player_forward_xz(app);
    Vector2 player_pos = { app->player.position.x, app->player.position.z };

    for (i = 0; i < app->entities.event_count; ++i) {
        DoorEvent *event = &app->entities.events[i];
        float score;

        if (!look_score(player_pos, forward, bt3d_door_center(event), &score)) continue;
        if (score > best_score) {
            best_score = score;
            best = event;
        }
    }

    return best;
}

static void format_marker_debug(AppState *app, const MarkerRuntime *marker, char *out, size_t out_size) {
    uint16_t tile = bt3d_tile_at(&app->content.map, marker->base.cell_x, marker->base.cell_y);
    snprintf(out, out_size, "Marker type %d  tile %04x  cell %d,%d",
        marker->base.type, tile, marker->base.cell_x, marker->base.cell_y);
}

/* The clipboard copy also lists the four neighbour wall flags. */
static void format_door_debug(AppState *app, const DoorEvent *door, int with_neighbors, char *out, size_t out_size) {
    static const uint8_t no_detail[4] = { 0 };
    int x = door->base.x;
    int y = door->base.y;
    uint16_t tile = bt3d_tile_at(&app->content.map, x, y);
    int slides_on_x_axis = door_slides_on_x_axis(app, x, y);
    const uint8_t *detail = bt3d_cell_in_bounds(x, y) ? &app->content.map.detail_grid[bt3d_tile_index_for(x, y) * 4] : no_detail;
    int length = snprintf(out, out_size,
        "%s  Door tile %04x  cell %d,%d  low %02x  family %04x  face %d  %s  %s  slices %d/%d  state %d  d %d,%d,%d,%d",
        app->content.map.name,
        tile,
        x, y,
        tile & 0xff,
        tile & 0xff00,
        (tile & 0x300) >> 8,
        slides_on_x_axis ? "x-slide" : "z-slide",
        bt3d_is_center_split_door_tile(tile) ? "split" : "full",
        bt3d_split_door_slice_index(tile, slides_on_x_axis, -0.25f),
        bt3d_split_door_slice_index(tile, slides_on_x_axis, 0.25f),
        door->base.state,
        detail[0], detail[1], detail[2], detail[3]);

    if (with_neighbors && length > 0 && (size_t)length < out_size) {
        snprintf(out + length, out_size - (size_t)length, "  n %d,%d,%d,%d",
            is_structural_wall_for_door_axis(app, x - 1, y),
            is_structural_wall_for_door_axis(app, x + 1, y),
            is_structural_wall_for_door_axis(app, x, y - 1),
            is_structural_wall_for_door_axis(app, x, y + 1));
    }
}

static void format_player_coordinates(AppState *app, char *out, size_t out_size) {
    snprintf(out, out_size, "%s  pos %.2f,%.2f  cell %d,%d  yaw %.1f",
        app->content.map.name,
        app->player.position.x, app->player.position.z,
        (int)floorf(app->player.position.x), (int)floorf(app->player.position.z),
        app->player.yaw * RAD2DEG);
}

static int copy_looked_at_marker_text(AppState *app) {
    MarkerRuntime *looked = get_looked_at_marker(app);
    char text[128];

    if (!looked) return 0;
    format_marker_debug(app, looked, text, sizeof(text));
    SetClipboardText(text);
    return 1;
}

static int copy_looked_at_door_text(AppState *app) {
    DoorEvent *door = get_looked_at_door(app);
    char text[320];

    if (!door) return 0;
    format_door_debug(app, door, 1, text, sizeof(text));
    SetClipboardText(text);
    return 1;
}

static int copy_player_coordinates_text(AppState *app) {
    char text[128];

    format_player_coordinates(app, text, sizeof(text));
    SetClipboardText(text);
    return 1;
}

void bt3d_debug_log(const char *fmt, ...) {
    char log_path[256];
    char buf[512];
    FILE *f;
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    bt3d_build_save_path("debug.log", log_path, sizeof(log_path));
    f = fopen(log_path, "a");
    if (f) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
}

int bt3d_copy_debug_target_text(AppState *app) {
    if (!copy_looked_at_door_text(app) && !copy_looked_at_marker_text(app)) {
        return copy_player_coordinates_text(app);
    }
    return 1;
}

static void draw_frame_time_graph(DebugState *debug) {
    enum { SAMPLE_COUNT = ARRAY_COUNT(debug->frame_ms) };
    Rectangle bounds = { (float)GetScreenWidth() - 344.0f, 36.0f, 324.0f, 112.0f };
    int refresh_rate = GetMonitorRefreshRate(GetCurrentMonitor());
    float target_ms;
    float slow_ms;
    float graph_max_ms;
    float frame_ms = GetFrameTime() * 1000.0f;
    float last_ms;
    float sum_ms = 0.0f;
    float max_ms = 0.0f;
    int count;
    int i;

    if (refresh_rate <= 0) refresh_rate = 60;
    target_ms = 1000.0f / (float)refresh_rate;
    slow_ms = target_ms * 2.0f;
    graph_max_ms = refresh_rate >= 100 ? 25.0f : 50.0f;

    if (frame_ms < 0.0f) frame_ms = 0.0f;
    if (debug->held_peak_timer > 0.0f) {
        debug->held_peak_timer -= GetFrameTime();
        if (debug->held_peak_timer <= 0.0f) {
            debug->held_peak_timer = 0.0f;
            debug->held_peak_ms = 0.0f;
        }
    }
    if (frame_ms > target_ms * 1.25f && frame_ms >= debug->held_peak_ms) {
        debug->held_peak_ms = frame_ms;
        debug->held_peak_timer = 1.5f;
    }

    debug->frame_ms[debug->frame_cursor] = frame_ms;
    debug->frame_cursor = (debug->frame_cursor + 1) % SAMPLE_COUNT;
    if (debug->frame_count < SAMPLE_COUNT) debug->frame_count++;
    count = debug->frame_count;
    last_ms = frame_ms;

    DrawRectangleRec(bounds, (Color){ 0, 0, 0, 190 });
    DrawRectangleLinesEx(bounds, 2.0f, debug->held_peak_timer > 0.0f ? (Color){ 255, 95, 75, 180 } : (Color){ 255, 255, 255, 95 });

    {
        float y60 = bounds.y + bounds.height - (target_ms / graph_max_ms) * bounds.height;
        float y30 = bounds.y + bounds.height - (slow_ms / graph_max_ms) * bounds.height;
        DrawLineEx((Vector2){ bounds.x, y60 }, (Vector2){ bounds.x + bounds.width, y60 }, 2.0f, (Color){ 80, 220, 120, 160 });
        DrawLineEx((Vector2){ bounds.x, y30 }, (Vector2){ bounds.x + bounds.width, y30 }, 2.0f, (Color){ 230, 180, 70, 160 });
        DrawText(TextFormat("%.1f", target_ms), (int)(bounds.x + bounds.width - 38.0f), (int)y60 - 15, 12, (Color){ 120, 245, 150, 210 });
        DrawText(TextFormat("%.1f", slow_ms), (int)(bounds.x + bounds.width - 38.0f), (int)y30 - 15, 12, (Color){ 255, 210, 90, 210 });
    }

    for (i = 0; i < count; ++i) {
        int sample_index = (debug->frame_cursor - count + i + SAMPLE_COUNT) % SAMPLE_COUNT;
        float ms = debug->frame_ms[sample_index];
        float t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
        float x = bounds.x + t * bounds.width;
        float clamped = fminf(ms, graph_max_ms);
        float h = (clamped / graph_max_ms) * bounds.height;
        Color color = ms > slow_ms ? (Color){ 255, 85, 70, 220 }
            : (ms > target_ms ? (Color){ 255, 205, 70, 220 } : (Color){ 90, 220, 130, 220 });
        DrawLineEx((Vector2){ x, bounds.y + bounds.height }, (Vector2){ x, bounds.y + bounds.height - h }, 2.0f, color);
        sum_ms += ms;
        if (ms > max_ms) max_ms = ms;
    }

    DrawText(TextFormat("%.1f ms  avg %.1f  max %.1f  %dHz", last_ms, count > 0 ? sum_ms / (float)count : 0.0f, max_ms, refresh_rate),
        (int)bounds.x + 8, (int)bounds.y + 8, 16, RAYWHITE);
    if (debug->held_peak_timer > 0.0f) {
        DrawText(TextFormat("drop %.1f ms", debug->held_peak_ms),
            (int)bounds.x + 8, (int)(bounds.y + bounds.height - 24.0f), 18, (Color){ 255, 120, 90, 255 });
    }
}

void bt3d_draw_debug_overlay(AppState *app) {
    /* dpad up/down/left/right, face A/B/X/Y, L/R/ZL/ZR, minus/plus */
    static const int watched_buttons[] = {
        GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN, GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_UP, GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
        GAMEPAD_BUTTON_LEFT_TRIGGER_1, GAMEPAD_BUTTON_RIGHT_TRIGGER_1, GAMEPAD_BUTTON_LEFT_TRIGGER_2, GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
        GAMEPAD_BUTTON_MIDDLE_LEFT, GAMEPAD_BUTTON_MIDDLE_RIGHT,
    };
    int down[ARRAY_COUNT(watched_buttons)];
    int dbg_gamepad;
    int text_top;
    int target_top;
    char text[320];
    int i;

    DrawText(TextFormat("FPS %d", GetFPS()), 12, 12, 20, (Color){ 240, 220, 80, 230 });
    draw_frame_time_graph(&app->debug);

    dbg_gamepad = bt3d_active_gamepad_index();
    for (i = 0; i < ARRAY_COUNT(watched_buttons); ++i) {
        down[i] = dbg_gamepad >= 0 ? IsGamepadButtonDown(dbg_gamepad, watched_buttons[i]) : 0;
    }
    text_top = (int)bt3d_hud_rect(app).height;

    format_player_coordinates(app, text, sizeof(text));
    DrawText(text, 16, text_top + 30, 18, LIGHTGRAY);
    DrawText(
        TextFormat("pad %d  %s  lx %.2f  ly %.2f  rx %.2f  ry %.2f  btn %d",
            dbg_gamepad,
            dbg_gamepad >= 0 ? GetGamepadName(dbg_gamepad) : "none",
            dbg_gamepad >= 0 ? GetGamepadAxisMovement(dbg_gamepad, GAMEPAD_AXIS_LEFT_X) : 0.0f,
            dbg_gamepad >= 0 ? GetGamepadAxisMovement(dbg_gamepad, GAMEPAD_AXIS_LEFT_Y) : 0.0f,
            dbg_gamepad >= 0 ? GetGamepadAxisMovement(dbg_gamepad, GAMEPAD_AXIS_RIGHT_X) : 0.0f,
            dbg_gamepad >= 0 ? GetGamepadAxisMovement(dbg_gamepad, GAMEPAD_AXIS_RIGHT_Y) : 0.0f,
            dbg_gamepad >= 0 ? GetGamepadButtonPressed() : -1),
        16, text_top + 52, 18, GREEN
    );
    DrawText(
        TextFormat("dpad %d%d%d%d  face %d%d%d%d  shoulder %d%d%d%d  mid %d%d",
            down[0], down[1], down[2], down[3],
            down[4], down[5], down[6], down[7],
            down[8], down[9], down[10], down[11],
            down[12], down[13]),
        16, text_top + 74, 18, GREEN
    );
    target_top = text_top + 96;
    if (bt3d_profiler_enabled()) {
        DrawText(TextFormat("prof frame %.2fms  avg %.2fms  peak %.2fms",
                bt3d_profiler_last_frame_ms(), bt3d_profiler_average_frame_ms(), bt3d_profiler_peak_frame_ms()),
            16, target_top, 18, YELLOW);
        target_top += 22;
    }
    {
        DoorEvent *looked_door = get_looked_at_door(app);
        MarkerRuntime *looked = get_looked_at_marker(app);
        if (looked_door) {
            format_door_debug(app, looked_door, 0, text, sizeof(text));
            DrawText(text, 16, target_top, 18, ORANGE);
        } else if (looked) {
            format_marker_debug(app, looked, text, sizeof(text));
            DrawText(text, 16, target_top, 18, SKYBLUE);
        }
    }
}
