#include "bt3d_input.h"

#include "bt3d_app_state.h"
#include "bt3d_math.h"

#include <math.h>

#if BT3D_PLATFORM_SWITCH
#include <switch.h>
#endif

#define DIRECTION_UP 1u
#define DIRECTION_DOWN 2u
#define DIRECTION_LEFT 4u
#define DIRECTION_RIGHT 8u

static const float MOVE_STICK_DEADZONE = 0.15f;
static const float LOOK_STICK_DEADZONE = 0.10f;
static const float MENU_STICK_THRESHOLD = 0.5f;
static const float MENU_REPEAT_INTERVAL = 0.2f;

static float deadzone(float value, float threshold) {
    return fabsf(value) < threshold ? 0.0f : value;
}

static unsigned stick_directions(Vector2 stick) {
    unsigned directions = 0;
    if (stick.y > MENU_STICK_THRESHOLD) directions |= DIRECTION_UP;
    if (stick.y < -MENU_STICK_THRESHOLD) directions |= DIRECTION_DOWN;
    if (stick.x < -MENU_STICK_THRESHOLD) directions |= DIRECTION_LEFT;
    if (stick.x > MENU_STICK_THRESHOLD) directions |= DIRECTION_RIGHT;
    return directions;
}

static void add_move_directions(FrameInput *input, unsigned directions) {
    if (directions & DIRECTION_UP) input->move.y += 1.0f;
    if (directions & DIRECTION_DOWN) input->move.y -= 1.0f;
    if (directions & DIRECTION_LEFT) input->move.x -= 1.0f;
    if (directions & DIRECTION_RIGHT) input->move.x += 1.0f;
}

#if BT3D_PLATFORM_SWITCH
/* libnx is read directly: raylib-nx's gamepad state is unreliable. */
void bt3d_init_input(AppState *app) {
    padInitializeDefault(&app->input.pad);
    padUpdate(&app->input.pad);
}

int bt3d_active_gamepad_index(void) {
    return 0;
}

void set_mouse_capture(AppState *app, int captured) {
    app->control.mouse_captured = captured;
}

/* Returns the directions held on the d-pad and left stick. */
static unsigned read_platform_input(InputState *state, FrameInput *input) {
    PadState *pad = &state->pad;
    u64 held;
    u64 pressed;
    HidAnalogStickState left_stick;
    Vector2 stick;
    unsigned dpad = 0;

    padUpdate(pad);
    held = padGetButtons(pad);
    pressed = padGetButtonsDown(pad);
    left_stick = padGetStickPos(pad, 0);
    stick = (Vector2){ (float)left_stick.x / 32767.0f, (float)left_stick.y / 32767.0f };
    input->look = deadzone((float)padGetStickPos(pad, 1).x / 32767.0f, LOOK_STICK_DEADZONE);

    if (held & HidNpadButton_Up) dpad |= DIRECTION_UP;
    if (held & HidNpadButton_Down) dpad |= DIRECTION_DOWN;
    if (held & HidNpadButton_Left) dpad |= DIRECTION_LEFT;
    if (held & HidNpadButton_Right) dpad |= DIRECTION_RIGHT;
    add_move_directions(input, dpad);
    input->move.x += deadzone(stick.x, MOVE_STICK_DEADZONE);
    input->move.y += deadzone(stick.y, MOVE_STICK_DEADZONE);

    input->pause = (pressed & HidNpadButton_Plus) != 0;
    input->fire = (pressed & HidNpadButton_ZR) != 0;
    input->use = (pressed & (HidNpadButton_A | HidNpadButton_ZL)) != 0;
    input->map = (pressed & HidNpadButton_X) != 0;
    input->prev_weapon = (pressed & HidNpadButton_L) != 0;
    input->next_weapon = (pressed & HidNpadButton_R) != 0;
    input->noclip = (pressed & HidNpadButton_Y) != 0;
    input->profiler = (pressed & HidNpadButton_Minus) != 0;
    input->prev_map = (pressed & HidNpadButton_Left) != 0;
    input->next_map = (pressed & HidNpadButton_Right) != 0;
    input->confirm = (pressed & HidNpadButton_A) != 0;
    input->back = (pressed & (HidNpadButton_B | HidNpadButton_Plus | HidNpadButton_Minus)) != 0;
    input->delete_entry = (pressed & HidNpadButton_Y) != 0;
    return dpad | stick_directions(stick);
}
#else
void bt3d_init_input(AppState *app) {
    (void)app;
}

int bt3d_active_gamepad_index(void) {
    int i;
    for (i = 0; i < 4; ++i) {
        if (IsGamepadAvailable(i)) return i;
    }
    return -1;
}

void set_mouse_capture(AppState *app, int captured) {
    app->control.mouse_captured = captured;
    if (captured) {
        DisableCursor();
    } else {
        EnableCursor();
    }
}

/* Edges are tracked here rather than with IsMouseButtonPressed, which some
   backends report like a held state. Sampled every frame, so a click that
   closes the menu does not also fire once play resumes. */
static int mouse_button_pressed(InputState *state, int button) {
    int down = IsMouseButtonDown(button);
    int pressed = down && !state->mouse_was_down[button];
    state->mouse_was_down[button] = down;
    return pressed;
}

static int pad_pressed(int gamepad, int button) {
    return gamepad >= 0 && IsGamepadButtonPressed(gamepad, button);
}

static int pad_down(int gamepad, int button) {
    return gamepad >= 0 && IsGamepadButtonDown(gamepad, button);
}

/* Returns the directions held on the keys, d-pad and left stick. */
static unsigned read_platform_input(InputState *state, FrameInput *input) {
    int gamepad = bt3d_active_gamepad_index();
    Vector2 stick = { 0 };
    unsigned dpad = 0;
    unsigned keys = 0;
    int ch;
    int i;

    if (gamepad >= 0) {
        /* Gamepad axes point down for up. */
        stick = (Vector2){ GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X), -GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y) };
        input->look = deadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X), LOOK_STICK_DEADZONE);
    }
    if (pad_down(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)) dpad |= DIRECTION_UP;
    if (pad_down(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) dpad |= DIRECTION_DOWN;
    if (pad_down(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) dpad |= DIRECTION_LEFT;
    if (pad_down(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) dpad |= DIRECTION_RIGHT;
    add_move_directions(input, dpad);
    input->move.x += deadzone(stick.x, MOVE_STICK_DEADZONE);
    input->move.y += deadzone(stick.y, MOVE_STICK_DEADZONE);

    /* The arrow keys turn rather than strafe. */
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) keys |= DIRECTION_UP;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) keys |= DIRECTION_DOWN;
    if (IsKeyDown(KEY_A)) keys |= DIRECTION_LEFT;
    if (IsKeyDown(KEY_D)) keys |= DIRECTION_RIGHT;
    add_move_directions(input, keys);
    if (IsKeyDown(KEY_LEFT)) {
        input->turn += 1.0f;
        keys |= DIRECTION_LEFT;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        input->turn -= 1.0f;
        keys |= DIRECTION_RIGHT;
    }

    input->mouse = GetMousePosition();
    input->mouse_delta = GetMouseDelta();
    input->mouse_moved = input->mouse_delta.x != 0.0f || input->mouse_delta.y != 0.0f;
    input->mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input->click = mouse_button_pressed(state, MOUSE_BUTTON_LEFT);

    input->pause = IsKeyPressed(KEY_ESCAPE) || pad_pressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    input->fire = input->click || IsKeyPressed(KEY_LEFT_CONTROL) || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
    input->use = mouse_button_pressed(state, MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)
        || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || pad_pressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
    input->map = IsKeyPressed(KEY_M) || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    input->prev_weapon = pad_pressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    input->next_weapon = pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    for (i = 1; i <= 4; ++i) {
        if (IsKeyPressed(KEY_ZERO + i)) input->weapon_slot = i;
    }
    input->quicksave = IsKeyPressed(KEY_F5);
    input->quickload = IsKeyPressed(KEY_F9);

    input->noclip = IsKeyPressed(KEY_N);
    input->profiler = IsKeyPressed(KEY_F10);
    input->prev_map = IsKeyPressed(KEY_LEFT_BRACKET) || pad_pressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    input->next_map = IsKeyPressed(KEY_RIGHT_BRACKET) || pad_pressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    input->copy_debug_text = IsKeyPressed(KEY_C);

    input->confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    input->back = IsKeyPressed(KEY_ESCAPE) || pad_pressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)
        || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    input->delete_entry = IsKeyPressed(KEY_BACKSPACE) || pad_pressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);

    while ((ch = GetCharPressed()) != 0) {
        if (ch >= 32 && ch <= 126 && input->typed_count < ARRAY_COUNT(input->typed)) input->typed[input->typed_count++] = ch;
    }
    input->erase = IsKeyPressed(KEY_BACKSPACE);
    return dpad | keys | stick_directions(stick);
}
#endif

void bt3d_read_frame_input(AppState *app, FrameInput *input) {
    InputState *state = &app->input;
    unsigned held;
    unsigned pressed;
    unsigned navigation;

    *input = (FrameInput){ 0 };
    held = read_platform_input(state, input);

    /* A direction steps once when pressed, then repeats while held. */
    pressed = held & ~state->held_directions;
    navigation = pressed;
    if (pressed) {
        state->repeat_timer = MENU_REPEAT_INTERVAL;
    } else if (held) {
        state->repeat_timer -= GetFrameTime();
        if (state->repeat_timer <= 0.0f) {
            navigation = held;
            state->repeat_timer = MENU_REPEAT_INTERVAL;
        }
    }
    state->held_directions = held;

    input->moved = pressed != 0;
    input->up = (navigation & DIRECTION_UP) != 0;
    input->down = (navigation & DIRECTION_DOWN) != 0;
    input->left = (navigation & DIRECTION_LEFT) != 0;
    input->right = (navigation & DIRECTION_RIGHT) != 0;
}
