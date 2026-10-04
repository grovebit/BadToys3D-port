#include "midi_player.h"
#include "bt3d_embedded_midi.h"
#include "bt3d_math.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TML_IMPLEMENTATION
#include "third_party/tinymidiloader/tml.h"

enum {
    MIDI_SAMPLE_RATE = 44100,
    MIDI_CHANNELS = 2,
    MIDI_BUFFER_FRAMES = 2048,
    MIDI_MAX_VOICES = 64
};

#define MENU_MUSIC_GAIN 0.65f

typedef enum {
    VOICE_MELODIC = 0,
    VOICE_KICK,
    VOICE_SNARE,
    VOICE_HAT,
    VOICE_TOM,
    VOICE_CYMBAL
} BuiltinVoiceKind;

typedef struct {
    int active;
    int releasing;
    int channel;
    int note;
    int program;
    BuiltinVoiceKind kind;
    float velocity;
    float phase;
    float phase2;
    float freq;
    float env;
    float pan;
    float age;
    /* Per-sample envelope multiplier, recomputed only when the decay rate changes. */
    float decay;
    float env_step;
} BuiltinVoice;

typedef struct {
    BuiltinVoice voices[MIDI_MAX_VOICES];
    int channel_program[16];
    float channel_volume[16];
    float channel_expression[16];
    float channel_pan[16];
    uint32_t noise_state;
} BuiltinSynth;

static float midi_note_freq(int note) {
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static float fast_noise(BuiltinSynth *synth) {
    synth->noise_state = synth->noise_state * 1664525u + 1013904223u;
    return ((float)((synth->noise_state >> 8) & 0xffffu) / 32767.5f) - 1.0f;
}

static float wave_saw(float phase) {
    return (phase / (PI * 2.0f)) * 2.0f - 1.0f;
}

static float wave_square(float phase) {
    return phase < PI ? 1.0f : -1.0f;
}

static void synth_init(BuiltinSynth *synth) {
    int i;

    memset(synth, 0, sizeof(*synth));
    synth->noise_state = 0x12345678u;
    for (i = 0; i < 16; ++i) {
        synth->channel_program[i] = 0;
        synth->channel_volume[i] = 1.0f;
        synth->channel_expression[i] = 1.0f;
        synth->channel_pan[i] = 0.5f;
    }
}

static BuiltinVoiceKind drum_kind_for_note(int note) {
    if (note == 35 || note == 36) return VOICE_KICK;
    if (note == 38 || note == 40) return VOICE_SNARE;
    if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) return VOICE_TOM;
    if (note == 42 || note == 44 || note == 46) return VOICE_HAT;
    return VOICE_CYMBAL;
}

static int find_voice_slot(BuiltinSynth *synth) {
    int i;
    int quietest = 0;
    float quietest_env = synth->voices[0].env;

    for (i = 0; i < MIDI_MAX_VOICES; ++i) {
        if (!synth->voices[i].active) return i;
        if (synth->voices[i].env < quietest_env) {
            quietest = i;
            quietest_env = synth->voices[i].env;
        }
    }
    return quietest;
}

static void synth_note_on(BuiltinSynth *synth, int channel, int note, int velocity) {
    int slot;
    BuiltinVoice *voice;

    if (!synth || channel < 0 || channel >= 16 || velocity <= 0) return;
    slot = find_voice_slot(synth);
    voice = &synth->voices[slot];
    memset(voice, 0, sizeof(*voice));
    voice->active = 1;
    voice->channel = channel;
    voice->note = note;
    voice->program = synth->channel_program[channel];
    voice->kind = channel == 9 ? drum_kind_for_note(note) : VOICE_MELODIC;
    voice->velocity = (float)velocity / 127.0f;
    voice->freq = midi_note_freq(note);
    voice->env = 1.0f;
    voice->pan = synth->channel_pan[channel];
}

static void synth_note_off(BuiltinSynth *synth, int channel, int note) {
    int i;

    if (!synth) return;
    for (i = 0; i < MIDI_MAX_VOICES; ++i) {
        BuiltinVoice *voice = &synth->voices[i];
        if (voice->active && voice->channel == channel && voice->note == note) {
            voice->releasing = 1;
        }
    }
}

static void synth_all_notes_off(BuiltinSynth *synth, int channel) {
    int i;

    if (!synth) return;
    for (i = 0; i < MIDI_MAX_VOICES; ++i) {
        if (synth->voices[i].active && (channel < 0 || synth->voices[i].channel == channel)) {
            synth->voices[i].releasing = 1;
        }
    }
}

static float melodic_sample_for_voice(BuiltinVoice *voice) {
    float s;

    switch (voice->program) {
        case 24: /* nylon guitar */
            s = 0.70f * sinf(voice->phase) + 0.25f * wave_saw(voice->phase2);
            break;
        case 33: /* fingered bass */
            s = 0.85f * sinf(voice->phase) + 0.15f * wave_square(voice->phase2);
            break;
        case 88: /* pad */
            s = 0.60f * sinf(voice->phase) + 0.40f * sinf(voice->phase2);
            break;
        case 94: /* synth lead */
            s = 0.55f * wave_square(voice->phase) + 0.35f * wave_saw(voice->phase2);
            break;
        case 99: /* atmosphere */
            s = 0.50f * sinf(voice->phase * 0.5f) + 0.35f * sinf(voice->phase2);
            break;
        case 0:
        default:
            s = 0.80f * sinf(voice->phase) + 0.18f * sinf(voice->phase2);
            break;
    }
    return s;
}

static float melodic_decay_for_program(int program) {
    switch (program) {
        case 24: return 1.8f;
        case 33: return 1.2f;
        case 88: return 0.25f;
        case 94: return 0.45f;
        case 99: return 0.18f;
        case 0:
        default: return 1.0f;
    }
}

static float render_voice(BuiltinSynth *synth, BuiltinVoice *voice, float dt) {
    float sample = 0.0f;
    float decay = 1.0f;

    if (!voice->active) return 0.0f;
    voice->age += dt;

    if (voice->kind == VOICE_MELODIC) {
        sample = melodic_sample_for_voice(voice);
        decay = voice->releasing ? 5.0f : melodic_decay_for_program(voice->program);
    } else if (voice->kind == VOICE_KICK) {
        float f = 42.0f + 90.0f * expf(-voice->age * 14.0f);
        voice->phase += (PI * 2.0f) * f * dt;
        sample = sinf(voice->phase) * 1.4f;
        decay = 8.5f;
    } else if (voice->kind == VOICE_SNARE) {
        sample = fast_noise(synth) * 0.75f + sinf(voice->phase) * 0.20f;
        decay = 10.0f;
    } else if (voice->kind == VOICE_HAT) {
        sample = fast_noise(synth) * 0.55f;
        decay = 22.0f;
    } else if (voice->kind == VOICE_TOM) {
        sample = sinf(voice->phase) * 0.9f;
        decay = 6.0f;
    } else {
        sample = fast_noise(synth) * 0.45f + sinf(voice->phase2) * 0.12f;
        decay = 5.0f;
    }

    if (voice->kind != VOICE_KICK) {
        voice->phase += (PI * 2.0f) * voice->freq * dt;
    }
    voice->phase2 += (PI * 2.0f) * voice->freq * 2.01f * dt;
    while (voice->phase >= PI * 2.0f) voice->phase -= PI * 2.0f;
    while (voice->phase2 >= PI * 2.0f) voice->phase2 -= PI * 2.0f;

    if (voice->env_step == 0.0f || decay != voice->decay) {
        voice->decay = decay;
        voice->env_step = expf(-decay * dt);
    }
    voice->env *= voice->env_step;
    if (voice->env < 0.0008f) {
        voice->active = 0;
        return 0.0f;
    }
    return sample * voice->env * voice->velocity;
}

static void synth_render(BuiltinSynth *synth, short *buffer, int offset_frames, int frame_count, int sample_rate) {
    int frame;
    float dt = 1.0f / (float)sample_rate;

    if (!synth || !buffer || frame_count <= 0) return;
    for (frame = 0; frame < frame_count; ++frame) {
        float left = 0.0f;
        float right = 0.0f;
        int i;

        for (i = 0; i < MIDI_MAX_VOICES; ++i) {
            BuiltinVoice *voice = &synth->voices[i];
            float amp;
            float sample;
            float pan;
            if (!voice->active) continue;

            sample = render_voice(synth, voice, dt);
            amp = synth->channel_volume[voice->channel] * synth->channel_expression[voice->channel] * 0.22f;
            pan = bt3d_clamp01(voice->pan);
            left += sample * amp * (1.0f - pan * 0.75f);
            right += sample * amp * (0.25f + pan * 0.75f);
        }

        left = bt3d_clamp_f(left, -1.0f, 1.0f);
        right = bt3d_clamp_f(right, -1.0f, 1.0f);
        buffer[(offset_frames + frame) * 2 + 0] = (short)(left * 32767.0f);
        buffer[(offset_frames + frame) * 2 + 1] = (short)(right * 32767.0f);
    }
}

static void midi_apply_message(BuiltinSynth *synth, const tml_message *message) {
    int channel;

    if (!synth || !message) return;
    channel = message->channel;
    if (channel < 0 || channel >= 16) return;
    switch (message->type) {
        case TML_NOTE_ON:
            if (message->velocity) {
                synth_note_on(synth, channel, message->key, message->velocity);
            } else {
                synth_note_off(synth, channel, message->key);
            }
            break;
        case TML_NOTE_OFF:
            synth_note_off(synth, channel, message->key);
            break;
        case TML_PROGRAM_CHANGE:
            synth->channel_program[channel] = message->program;
            break;
        case TML_CONTROL_CHANGE:
            if (message->control == 7) {
                synth->channel_volume[channel] = (float)message->control_value / 127.0f;
            } else if (message->control == 10) {
                synth->channel_pan[channel] = (float)message->control_value / 127.0f;
            } else if (message->control == 11) {
                synth->channel_expression[channel] = (float)message->control_value / 127.0f;
            } else if (message->control == 120 || message->control == 123) {
                synth_all_notes_off(synth, channel);
            }
            break;
        default:
            break;
    }
}

static void midi_reset_song(MidiPlayer *player) {
    BuiltinSynth *synth = (BuiltinSynth *)player->synth;

    if (!synth) return;
    synth_init(synth);
    player->next_message = player->messages;
    player->time_ms = 0.0;
}

static void midi_render_frames(MidiPlayer *player, short *buffer, int frame_count) {
    BuiltinSynth *synth = (BuiltinSynth *)player->synth;
    tml_message *message;
    int frame_cursor = 0;

    if (!player || !synth || !buffer || frame_count <= 0) return;
    memset(buffer, 0, (size_t)frame_count * MIDI_CHANNELS * sizeof(short));

    message = (tml_message *)player->next_message;
    while (frame_cursor < frame_count) {
        int next_event_frame = frame_count;
        double buffer_start_ms = player->time_ms;
        double buffer_end_ms = buffer_start_ms + ((double)frame_count * 1000.0 / (double)MIDI_SAMPLE_RATE);

        if (message && (double)message->time < buffer_end_ms) {
            double event_delta_ms = (double)message->time - buffer_start_ms;
            if (event_delta_ms < 0.0) event_delta_ms = 0.0;
            next_event_frame = (int)((event_delta_ms * (double)MIDI_SAMPLE_RATE) / 1000.0);
            if (next_event_frame < frame_cursor) next_event_frame = frame_cursor;
            if (next_event_frame > frame_count) next_event_frame = frame_count;
        }

        if (message && next_event_frame <= frame_cursor) {
            midi_apply_message(synth, message);
            message = message->next;
            continue;
        }

        if (next_event_frame > frame_cursor) {
            synth_render(synth, buffer, frame_cursor, next_event_frame - frame_cursor, MIDI_SAMPLE_RATE);
            frame_cursor = next_event_frame;
        }

        while (message && (double)message->time <= buffer_start_ms + ((double)frame_cursor * 1000.0 / (double)MIDI_SAMPLE_RATE)) {
            midi_apply_message(synth, message);
            message = message->next;
        }

        if (!message && frame_cursor < frame_count) {
            synth_render(synth, buffer, frame_cursor, frame_count - frame_cursor, MIDI_SAMPLE_RATE);
            frame_cursor = frame_count;
        }
    }

    if (!message) {
        midi_reset_song(player);
    } else {
        player->next_message = message;
        player->time_ms += ((double)frame_count * 1000.0 / (double)MIDI_SAMPLE_RATE);
    }
}

int bt3d_midi_player_start(MidiPlayer *player) {
    float volume;
    if (!player || bt3d_menu_midi_size == 0) return 0;
    volume = bt3d_clamp01(player->volume);
    bt3d_midi_player_stop(player);
    player->volume = volume;

    player->synth = calloc(1, sizeof(BuiltinSynth));
    if (!player->synth) return 0;
    synth_init((BuiltinSynth *)player->synth);

    player->messages = tml_load_memory(bt3d_menu_midi_data, (int)bt3d_menu_midi_size);
    if (!player->messages) {
        bt3d_midi_player_stop(player);
        return 0;
    }

    player->buffer = (short *)calloc((size_t)MIDI_BUFFER_FRAMES * MIDI_CHANNELS, sizeof(short));
    if (!player->buffer) {
        bt3d_midi_player_stop(player);
        return 0;
    }

    SetAudioStreamBufferSizeDefault(MIDI_BUFFER_FRAMES);
    player->stream = LoadAudioStream(MIDI_SAMPLE_RATE, 16, MIDI_CHANNELS);
    if (!player->stream.buffer) {
        bt3d_midi_player_stop(player);
        return 0;
    }
    SetAudioStreamVolume(player->stream, MENU_MUSIC_GAIN * player->volume);
    player->next_message = player->messages;
    player->time_ms = 0.0;
    PlayAudioStream(player->stream);
    player->playing = 1;

    midi_render_frames(player, player->buffer, MIDI_BUFFER_FRAMES);
    UpdateAudioStream(player->stream, player->buffer, MIDI_BUFFER_FRAMES);
    return 1;
}

void bt3d_midi_player_stop(MidiPlayer *player) {
    if (!player) return;
    if (player->playing) {
        StopAudioStream(player->stream);
        UnloadAudioStream(player->stream);
    }
    free(player->buffer);
    if (player->messages) tml_free((tml_message *)player->messages);
    free(player->synth);
    memset(player, 0, sizeof(*player));
}

void bt3d_midi_player_update(MidiPlayer *player) {
    int buffers_updated = 0;
    if (!player || !player->playing) return;
    while (buffers_updated < 4 && IsAudioStreamProcessed(player->stream)) {
        midi_render_frames(player, player->buffer, MIDI_BUFFER_FRAMES);
        UpdateAudioStream(player->stream, player->buffer, MIDI_BUFFER_FRAMES);
        buffers_updated++;
    }
}

void bt3d_midi_player_set_volume(MidiPlayer *player, float volume) {
    if (!player) return;
    volume = bt3d_clamp01(volume);
    player->volume = volume;
    if (player->playing) {
        SetAudioStreamVolume(player->stream, MENU_MUSIC_GAIN * volume);
    }
}
