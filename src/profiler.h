#ifndef BT3D_PROFILER_H
#define BT3D_PROFILER_H

#define BT3D_PROFILER_MAX_ZONES 64
#define BT3D_PROFILER_MAX_STACK 16

/* Slow-frame trigger: any frame slower than this logs a zone breakdown.
   The 60 fps budget is 16.667 ms, so this only flags missed frames. */
#define BT3D_PROFILER_SLOW_MS 16.7

int  bt3d_profiler_enabled(void);
void bt3d_profiler_toggle(void);
double bt3d_profiler_last_frame_ms(void);
double bt3d_profiler_average_frame_ms(void);
double bt3d_profiler_peak_frame_ms(void);

void bt3d_profiler_begin_frame(void);
void bt3d_profiler_end_frame(void);
void bt3d_profiler_zone_begin(const char *name);
void bt3d_profiler_zone_end(const char *name);

#define BT3D_PROF_BEGIN(name) bt3d_profiler_zone_begin(name)
#define BT3D_PROF_END(name)   bt3d_profiler_zone_end(name)

#endif
