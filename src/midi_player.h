#ifndef BT3D_MIDI_PLAYER_H
#define BT3D_MIDI_PLAYER_H

#include "raylib.h"

typedef struct MidiPlayer {
    AudioStream stream;
    int playing;
    void *synth;
    void *messages;
    void *next_message;
    short *buffer;
    double time_ms;
    float volume;
} MidiPlayer;

int bt3d_midi_player_start(MidiPlayer *player);
void bt3d_midi_player_stop(MidiPlayer *player);
void bt3d_midi_player_update(MidiPlayer *player);
void bt3d_midi_player_set_volume(MidiPlayer *player, float volume);

#endif
