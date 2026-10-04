#include "bt3d_runtime.h"

#include "bt3d_assets.h"
#include "bt3d_debug.h"
#include "bt3d_input.h"
#include "bt3d_menu.h"
#include "bt3d_platform.h"
#include "bt3d_player.h"
#include "bt3d_render.h"
#include "bt3d_session.h"
#include "profiler.h"
#include "rlgl.h"

#include <stdlib.h>

int bt3d_runtime_init(Bt3dRuntime *runtime) {
    AppState *app = &runtime->app;

    bt3d_reset_app_state(app);
    bt3d_enter_game_directory();
    bt3d_load_config(app);
    bt3d_init_window_and_audio(app);
    if (!IsWindowReady()) return 0;
    bt3d_init_input(app);
    runtime->camera = (Camera3D){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f, CAMERA_PERSPECTIVE };
#if !BT3D_PLATFORM_SWITCH
    /* Tight clip planes give the depth buffer enough precision to keep
       floor / wall junctions from z-fighting, matching the browser port
       which uses near=0.05 / far=256. raylib-nx's older rlgl does not
       expose rlSetClipPlanes. */
    rlSetClipPlanes(0.05, 128.0);
#endif
#if !BT3D_PLATFORM_SWITCH && !BT3D_PLATFORM_WEB
    SetTargetFPS(BT3D_TARGET_FPS);
#endif

    bt3d_load_initial_pack(app);
    bt3d_open_menu(app);
    return 1;
}

int bt3d_runtime_should_close(const Bt3dRuntime *runtime) {
    return WindowShouldClose() || runtime->app.menu.quit_requested;
}

void bt3d_runtime_frame(Bt3dRuntime *runtime) {
    AppState *app = &runtime->app;
    FrameInput input;

    bt3d_read_frame_input(app, &input);
    if (app->settings.debug_tools_enabled && app->session.in_session && !app->menu.active && app->debug.perf_frame++ % 60 == 0) {
        bt3d_debug_log("fps=%d  map=%s  pos=%.1f,%.1f  enemies=%d  events=%d  markers=%d",
            GetFPS(), app->content.map.name, app->player.position.x, app->player.position.z,
            app->entities.enemy_count, app->entities.event_count, app->entities.marker_count);
    }
    bt3d_profiler_begin_frame();
    if (app->content.pack.entry_count == 0) {
        /* Without the data pack there is only a message to dismiss. */
        app->menu.quit_requested |= input.back;
    } else {
        BT3D_PROF_BEGIN("update_player");
        bt3d_update_player(app, &input, &runtime->camera);
        BT3D_PROF_END("update_player");
        bt3d_update_menu_music(app);
    }
    bt3d_draw_frame(app, runtime->camera);
}

void bt3d_runtime_shutdown(Bt3dRuntime *runtime) {
    AppState *app = &runtime->app;

    bt3d_unload_assets(app);
    bt3d_midi_player_stop(&app->audio.menu_midi);
    bt3d_free_entities(app);
    free(app->session.map_entries);
    map242_unload(&app->content.map);
    datpack_unload(&app->content.pack);
    if (app->audio.initialized) CloseAudioDevice();
    CloseWindow();
}
