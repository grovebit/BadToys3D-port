#ifndef BT3D_AUDIO_H
#define BT3D_AUDIO_H

typedef struct AppState AppState;

enum {
    BT3D_SOUND_UI_MOVE = 4,
    BT3D_SOUND_UI_CONFIRM = 13,
    BT3D_SOUND_LEVEL_EXIT = 27,
    BT3D_SOUND_UI_BACK = 29
};

void play_sound_id(AppState *app, int sound_id, float volume);
void play_sound_id_restart(AppState *app, int sound_id, float volume);

#endif
