#include "profiler.h"

#include "bt3d_math.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    double frame_seconds;
    int frame_calls;
} ProfZone;

typedef struct {
    int zone_index;
    double start_time;
} ProfStackFrame;

static ProfZone g_zones[BT3D_PROFILER_MAX_ZONES];
static int g_zone_count = 0;
static ProfStackFrame g_stack[BT3D_PROFILER_MAX_STACK];
static int g_stack_top = 0;
static double g_frame_start = 0.0;
static int g_enabled = 0;
static int g_frame_counter = 0;
static double g_last_log_time = 0.0;
static double g_frame_samples[120];
static int g_frame_sample_cursor = 0;
static int g_frame_sample_count = 0;
static double g_last_frame_ms = 0.0;
static double g_peak_frame_ms = 0.0;

static void reset_frame_stats(void) {
    memset(g_frame_samples, 0, sizeof(g_frame_samples));
    g_frame_sample_cursor = 0;
    g_frame_sample_count = 0;
    g_last_frame_ms = 0.0;
    g_peak_frame_ms = 0.0;
    g_frame_counter = 0;
    g_last_log_time = 0.0;
}

static int find_or_create_zone(const char *name) {
    int i;
    for (i = 0; i < g_zone_count; ++i) {
        if (g_zones[i].name == name) return i;
    }
    for (i = 0; i < g_zone_count; ++i) {
        if (g_zones[i].name && strcmp(g_zones[i].name, name) == 0) {
            g_zones[i].name = name;
            return i;
        }
    }
    if (g_zone_count >= BT3D_PROFILER_MAX_ZONES) return -1;
    i = g_zone_count++;
    memset(&g_zones[i], 0, sizeof(g_zones[i]));
    g_zones[i].name = name;
    return i;
}

static void profiler_set_enabled(int enabled) {
    g_enabled = enabled ? 1 : 0;
    if (g_enabled) {
        reset_frame_stats();
        printf("[prof] enabled  slow>%.1fms\n", BT3D_PROFILER_SLOW_MS);
        fflush(stdout);
    } else {
        printf("[prof] disabled\n");
        fflush(stdout);
    }
}

int bt3d_profiler_enabled(void) {
    return g_enabled;
}

void bt3d_profiler_toggle(void) {
    profiler_set_enabled(!g_enabled);
}

double bt3d_profiler_last_frame_ms(void) {
    return g_last_frame_ms;
}

double bt3d_profiler_average_frame_ms(void) {
    double total = 0.0;
    int i = 0;
    if (g_frame_sample_count <= 0) return 0.0;
    for (i = 0; i < g_frame_sample_count; ++i) {
        total += g_frame_samples[i];
    }
    return total / (double)g_frame_sample_count;
}

double bt3d_profiler_peak_frame_ms(void) {
    return g_peak_frame_ms;
}

void bt3d_profiler_begin_frame(void) {
    int i;
    if (!g_enabled) return;
    for (i = 0; i < g_zone_count; ++i) {
        g_zones[i].frame_seconds = 0.0;
        g_zones[i].frame_calls = 0;
    }
    g_stack_top = 0;
    g_frame_start = GetTime();
}

static int cmp_zone_desc(const void *a, const void *b) {
    const ProfZone *za = (const ProfZone *)a;
    const ProfZone *zb = (const ProfZone *)b;
    if (za->frame_seconds < zb->frame_seconds) return 1;
    if (za->frame_seconds > zb->frame_seconds) return -1;
    return 0;
}

void bt3d_profiler_end_frame(void) {
    double now;
    double frame_ms;
    ProfZone sorted[BT3D_PROFILER_MAX_ZONES];
    int i;
    int count;
    char line[768];
    int pos;
    int written;

    if (!g_enabled) return;

    now = GetTime();
    frame_ms = (now - g_frame_start) * 1000.0;
    g_last_frame_ms = frame_ms;
    if (frame_ms > g_peak_frame_ms) {
        g_peak_frame_ms = frame_ms;
    }
    g_frame_samples[g_frame_sample_cursor] = frame_ms;
    g_frame_sample_cursor = (g_frame_sample_cursor + 1) % ARRAY_COUNT(g_frame_samples);
    if (g_frame_sample_count < ARRAY_COUNT(g_frame_samples)) {
        g_frame_sample_count++;
    }
    g_frame_counter++;

    if (frame_ms < BT3D_PROFILER_SLOW_MS) return;

    /* Rate-limit sustained slowdowns: at most one log per 100 ms so the
       console stays readable over nxlink while the slow stretch lasts. */
    if (now - g_last_log_time < 0.1) return;
    g_last_log_time = now;

    count = g_zone_count;
    memcpy(sorted, g_zones, sizeof(ProfZone) * (size_t)count);
    qsort(sorted, (size_t)count, sizeof(ProfZone), cmp_zone_desc);

    pos = 0;
    written = snprintf(line + pos, sizeof(line) - pos,
        "[prof] frame #%d  %.2fms  fps %d  ",
        g_frame_counter, frame_ms, GetFPS());
    if (written > 0) pos += written;

    for (i = 0; i < count && pos < (int)sizeof(line) - 16; ++i) {
        double ms = sorted[i].frame_seconds * 1000.0;
        if (ms < 0.05) continue;
        written = snprintf(line + pos, sizeof(line) - pos,
            "%s %.2f%s ",
            sorted[i].name ? sorted[i].name : "?", ms,
            sorted[i].frame_calls > 1 ? "*" : "");
        if (written <= 0) break;
        pos += written;
    }

    if (pos >= (int)sizeof(line)) pos = (int)sizeof(line) - 1;
    line[pos] = '\0';
    printf("%s\n", line);
    fflush(stdout);
}

void bt3d_profiler_zone_begin(const char *name) {
    int idx;
    if (!g_enabled || !name) return;
    idx = find_or_create_zone(name);
    if (idx < 0) return;
    if (g_stack_top >= BT3D_PROFILER_MAX_STACK) return;
    g_stack[g_stack_top].zone_index = idx;
    g_stack[g_stack_top].start_time = GetTime();
    g_stack_top++;
}

void bt3d_profiler_zone_end(const char *name) {
    int idx;
    double elapsed;
    (void)name;
    if (!g_enabled) return;
    if (g_stack_top <= 0) return;
    g_stack_top--;
    idx = g_stack[g_stack_top].zone_index;
    if (idx < 0 || idx >= g_zone_count) return;
    elapsed = GetTime() - g_stack[g_stack_top].start_time;
    g_zones[idx].frame_seconds += elapsed;
    g_zones[idx].frame_calls += 1;
}
