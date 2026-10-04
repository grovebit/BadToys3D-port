#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_math.h"

static Sound *sound_for_id(AppState *app, int sound_id) {
    Sound *sound;
    if (sound_id <= 0 || sound_id >= ARRAY_COUNT(app->assets.sounds)) return NULL;
    sound = &app->assets.sounds[sound_id];
    return sound->frameCount ? sound : NULL;
}

void play_sound_id(AppState *app, int sound_id, float volume) {
    Sound *sound = sound_for_id(app, sound_id);
    if (!sound) return;
    SetSoundVolume(*sound, volume * app->settings.sound_volume);
    PlaySound(*sound);
}

void play_sound_id_restart(AppState *app, int sound_id, float volume) {
    Sound *sound = sound_for_id(app, sound_id);
    if (!sound) return;
    SetSoundVolume(*sound, volume * app->settings.sound_volume);
    if (IsSoundPlaying(*sound)) StopSound(*sound);
    PlaySound(*sound);
}
