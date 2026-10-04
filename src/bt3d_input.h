#ifndef BT3D_INPUT_H
#define BT3D_INPUT_H

#include "raylib.h"

typedef struct AppState AppState;

/* Everything the game reads from the keyboard, mouse and gamepads, sampled
   once per frame. Pressed flags are set on the frame a button goes down. */
typedef struct {
    /* Gameplay, held. */
    Vector2 move;          /* x strafes right, y walks forward; each -1..1 */
    float turn;            /* keyboard turning, +1 to the left */
    float look;            /* look stick x past its deadzone, +1 to the right */
    Vector2 mouse_delta;

    /* Gameplay, pressed. */
    int pause;
    int fire;
    int use;
    int map;
    int moved;             /* a direction was pressed; closes the automap */
    int prev_weapon;
    int next_weapon;
    int weapon_slot;       /* 1-4 from the number keys, else 0 */
    int quicksave;
    int quickload;

    /* Debug tools, pressed. */
    int noclip;
    int profiler;
    int prev_map;
    int next_map;
    int copy_debug_text;

    /* Menus: directions also repeat while held. */
    int up;
    int down;
    int left;
    int right;
    int confirm;
    int back;
    int delete_entry;

    /* Text entry. */
    int typed[8];
    int typed_count;
    int erase;

    /* Mouse pointer; still on the Switch. */
    Vector2 mouse;
    int mouse_moved;
    int mouse_down;
    int click;
} FrameInput;

void bt3d_init_input(AppState *app);
void bt3d_read_frame_input(AppState *app, FrameInput *input);
/* The gamepad desktop input reads, or -1; always 0 on the Switch. */
int bt3d_active_gamepad_index(void);
void set_mouse_capture(AppState *app, int captured);

#endif
