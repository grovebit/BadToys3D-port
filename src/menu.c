#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_platform.h"
#include "bt3d_save_slots.h"
#include "bt3d_savegame.h"
#include "bt3d_session.h"
#include "menu_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if BT3D_PLATFORM_SWITCH
#define PLATFORM_TEXT(pad_text, keyboard_text) (pad_text)
#else
#define PLATFORM_TEXT(pad_text, keyboard_text) (keyboard_text)
#endif

static void add_item(MenuPage *page, MenuAction action, int arg, const char *label) {
    MenuItem *item = &page->items[page->count++];
    item->action = action;
    item->arg = arg;
    snprintf(item->label, sizeof(item->label), "%s", label);
    item->rect = (Rectangle){ 0 };
}

/* Sizes the panel for the title and `row_count` rows; returns the first row. */
static Rectangle layout_panel(MenuPage *page, int row_count, float *row_step) {
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    float width = fminf(620.0f, fmaxf(360.0f, screen_width * 0.42f));
    float row_height = fminf(42.0f, fmaxf(28.0f, screen_height * 0.05f));
    float padding_x = fminf(40.0f, fmaxf(24.0f, width * 0.07f));
    float padding_top = fminf(28.0f, fmaxf(18.0f, screen_height * 0.03f));
    float padding_bottom = fminf(32.0f, fmaxf(20.0f, screen_height * 0.035f));
    float height = fminf(padding_top + row_height * (float)(row_count + 1) + padding_bottom, screen_height * 0.72f);
    float x = (screen_width - width) * 0.5f;
    float y = fmaxf(28.0f, (screen_height - height) * 0.5f);

    page->panel = (Rectangle){ x, y, width, height };
    page->title_position = (Vector2){ x + padding_x, y + 18.0f };
    *row_step = row_height;
    return (Rectangle){ x + padding_x, y + padding_top + row_height - 4.0f, width - padding_x * 2.0f, row_height - 4.0f };
}

static Rectangle row_rect(Rectangle first_row, float row_step, int row) {
    first_row.y += row_step * (float)row;
    return first_row;
}

static void layout_page(const AppState *app, MenuPage *page) {
    Rectangle first_row;
    float row_step;
    int i;

    if (app->menu.screen == MENU_SCREEN_LOAD) {
        /* A window of saves, with Back always below it. */
        int visible = bt3d_min_i(app->saves.count - app->menu.scroll, MENU_VISIBLE_SAVES);
        first_row = layout_panel(page, visible + 1, &row_step);
        for (i = 0; i < visible; ++i) {
            page->items[app->menu.scroll + i].rect = row_rect(first_row, row_step, i);
        }
        page->items[page->count - 1].rect = row_rect(first_row, row_step, visible);
    } else if (app->menu.screen == MENU_SCREEN_SAVE_DIALOG) {
        /* The name field takes the first row. */
        first_row = layout_panel(page, page->count + 1, &row_step);
        page->name_box = first_row;
        for (i = 0; i < page->count; ++i) page->items[i].rect = row_rect(first_row, row_step, i + 1);
    } else {
        first_row = layout_panel(page, page->count, &row_step);
        for (i = 0; i < page->count; ++i) page->items[i].rect = row_rect(first_row, row_step, i);
    }
}

static void add_options_items(const AppState *app, MenuPage *page) {
    const AppSettings *settings = &app->settings;

    add_item(page, MENU_ITEM_SENSITIVITY, 0, TextFormat("%s Sensitivity: %.1f", PLATFORM_TEXT("Turn", "Mouse"), settings->mouse_sensitivity));
    add_item(page, MENU_ITEM_MUSIC, 0, TextFormat("Music Volume: %d%%", (int)roundf(settings->music_volume * 100.0f)));
    add_item(page, MENU_ITEM_SOUND, 0, TextFormat("Sound Volume: %d%%", (int)roundf(settings->sound_volume * 100.0f)));
#if !BT3D_PLATFORM_SWITCH && !BT3D_PLATFORM_WEB && !BT3D_PLATFORM_ANDROID
    add_item(page, MENU_ITEM_DISPLAY, 0, settings->display_mode == BT3D_DISPLAY_WINDOWED ? "Display: Windowed" : "Display: Borderless");
#endif
    add_item(page, MENU_ITEM_DEBUG, 0, settings->debug_tools_enabled ? "Debug Tools: On" : "Debug Tools: Off");
    if (settings->debug_tools_enabled) {
        add_item(page, MENU_ITEM_GOD, 0, app->control.god_mode_enabled ? "God Mode: On" : "God Mode: Off");
        add_item(page, MENU_ITEM_UNLOCK, 0, "Unlock Weapons");
        add_item(page, MENU_ITEM_KEYS, 0, "Give All Keys");
    }
    add_item(page, MENU_ITEM_BACK, 0, "Back");
}

void menu_build_page(const AppState *app, MenuPage *page) {
    int i;

    page->count = 0;
    page->hint = NULL;
    switch (app->menu.screen) {
        case MENU_SCREEN_CAMPAIGN:
            page->title = "Campaign";
            page->hint = PLATFORM_TEXT("D-Pad to choose, A to select, B to return", "Up/Down to choose, Enter to select, Esc to return");
            add_item(page, MENU_ITEM_NEW_GAME, 0, "New Game");
            add_item(page, MENU_ITEM_OPEN_LOAD, 0, "Load Game");
            break;
        case MENU_SCREEN_DIFFICULTY:
            page->title = "Select Difficulty";
            page->hint = PLATFORM_TEXT("D-Pad to choose, A to start, B to cancel", "Up/Down to choose, Enter to start, Esc to cancel");
            add_item(page, MENU_ITEM_START, BT3D_DIFFICULTY_EASY, "Easy");
            add_item(page, MENU_ITEM_START, BT3D_DIFFICULTY_MEDIUM, "Medium");
            add_item(page, MENU_ITEM_START, BT3D_DIFFICULTY_HARD, "Hard");
            add_item(page, MENU_ITEM_BACK, 0, "Back");
            break;
        case MENU_SCREEN_LOAD:
            page->title = "Load Game";
            page->hint = PLATFORM_TEXT("A load  Y delete selected  B back", "Enter load  Backspace delete selected  Esc back");
            for (i = 0; i < app->saves.count; ++i) add_item(page, MENU_ITEM_LOAD, i, app->saves.entries[i].label);
            add_item(page, MENU_ITEM_BACK, 0, "Back");
            break;
        case MENU_SCREEN_SAVE_DIALOG:
            page->title = "Save Game";
            page->hint = PLATFORM_TEXT(NULL, "Type a name, Enter to confirm, Esc to cancel");
            add_item(page, MENU_ITEM_SAVE, 0, "Save");
            add_item(page, MENU_ITEM_BACK, 0, "Cancel");
            break;
        case MENU_SCREEN_OPTIONS:
            page->title = "Options";
            page->hint = PLATFORM_TEXT("Adjust with D-Pad Left/Right, A to toggle, B to return",
                "Adjust with Left/Right or drag sliders, Enter toggles/selects, Esc to return");
            add_options_items(app, page);
            break;
        default:
            page->title = "Main Menu";
            if (app->session.in_session) {
                add_item(page, MENU_ITEM_CONTINUE, 0, "Continue");
                add_item(page, MENU_ITEM_OPEN_SAVE, 0, "Save Game");
                add_item(page, MENU_ITEM_OPEN_OPTIONS, 0, "Options");
                add_item(page, MENU_ITEM_EXIT_SESSION, 0, "Exit to Menu");
            } else {
                add_item(page, MENU_ITEM_OPEN_CAMPAIGN, 0, "Campaign");
                add_item(page, MENU_ITEM_OPEN_OPTIONS, 0, "Options");
#if !BT3D_PLATFORM_WEB
                /* The page owns the web build's lifetime. */
                add_item(page, MENU_ITEM_QUIT, 0, "Quit");
#endif
            }
            break;
    }
    layout_page(app, page);
}

Rectangle menu_slider_rect(Rectangle item) {
    float width = fminf(220.0f, fmaxf(130.0f, item.width * 0.38f));
    return (Rectangle){ item.x + item.width - width - 16.0f, item.y + item.height * 0.5f - 4.0f, width, 8.0f };
}

static void play_confirm(AppState *app) {
    play_sound_id(app, BT3D_SOUND_UI_CONFIRM, 0.55f);
}

static void play_back(AppState *app) {
    play_sound_id(app, BT3D_SOUND_UI_BACK, 0.45f);
}

static void play_error(AppState *app) {
    play_sound_id(app, BT3D_SOUND_UI_BACK, 0.5f);
}

static void play_move(AppState *app) {
    play_sound_id_restart(app, BT3D_SOUND_UI_MOVE, 0.4f);
}

static void set_screen(AppState *app, int screen) {
    app->menu.screen = screen;
    app->menu.index = 0;
    app->menu.scroll = 0;
}

void bt3d_open_menu(AppState *app) {
    app->menu.active = 1;
    set_screen(app, MENU_SCREEN_MAIN);
    set_mouse_capture(app, 0);
}

static void close_menu(AppState *app) {
    app->menu.active = 0;
    set_mouse_capture(app, 1);
}

static void go_back(AppState *app) {
    if (app->menu.screen != MENU_SCREEN_MAIN) {
        if (app->menu.screen == MENU_SCREEN_OPTIONS) bt3d_save_config(app);
        play_back(app);
        set_screen(app, MENU_SCREEN_MAIN);
    } else if (app->session.in_session) {
        play_back(app);
        close_menu(app);
    }
}

static void set_debug_tools_enabled(AppState *app, int enabled) {
    app->settings.debug_tools_enabled = enabled;
    if (!enabled) {
        app->control.noclip_enabled = 0;
        app->control.god_mode_enabled = 0;
    }
}

static int step_value(float *value, float step, float min_value, float max_value) {
    float previous = *value;
    *value = fminf(fmaxf(*value + step, min_value), max_value);
    return *value != previous;
}

/* Left/Right steps a value or flips a toggle. */
static void adjust_option(AppState *app, MenuAction action, int direction) {
    AppSettings *settings = &app->settings;
    float step = 0.1f * (float)direction;
    int changed = 1;

    switch (action) {
        case MENU_ITEM_SENSITIVITY:
            changed = step_value(&settings->mouse_sensitivity, step, MOUSE_SENSITIVITY_MIN, MOUSE_SENSITIVITY_MAX);
            break;
        case MENU_ITEM_MUSIC:
            changed = step_value(&settings->music_volume, step, 0.0f, 1.0f);
            break;
        case MENU_ITEM_SOUND:
            changed = step_value(&settings->sound_volume, step, 0.0f, 1.0f);
            break;
        case MENU_ITEM_DISPLAY:
            bt3d_toggle_display_mode(app);
            break;
        case MENU_ITEM_DEBUG:
            set_debug_tools_enabled(app, !settings->debug_tools_enabled);
            break;
        default:
            changed = 0;
            break;
    }
    if (changed) play_move(app);
}

static void activate(AppState *app, const MenuItem *item) {
    int ok = 1;
    int i;

    switch (item->action) {
        case MENU_ITEM_CONTINUE:
            close_menu(app);
            break;
        case MENU_ITEM_OPEN_SAVE:
            default_save_name_with_level(app, app->menu.save_name_input, sizeof(app->menu.save_name_input));
            app->menu.save_name_length = (int)strlen(app->menu.save_name_input);
            set_screen(app, MENU_SCREEN_SAVE_DIALOG);
            break;
        case MENU_ITEM_OPEN_OPTIONS:
            set_screen(app, MENU_SCREEN_OPTIONS);
            break;
        case MENU_ITEM_EXIT_SESSION:
            app->session.in_session = 0;
            set_screen(app, MENU_SCREEN_MAIN);
            break;
        case MENU_ITEM_OPEN_CAMPAIGN:
            set_screen(app, MENU_SCREEN_CAMPAIGN);
            break;
        case MENU_ITEM_QUIT:
            app->menu.quit_requested = 1;
            break;
        case MENU_ITEM_NEW_GAME:
            set_screen(app, MENU_SCREEN_DIFFICULTY);
            app->menu.index = app->session.difficulty;
            break;
        case MENU_ITEM_OPEN_LOAD:
            /* The load screen is the only reader of the save list. */
            refresh_save_entries(app);
            ok = app->saves.count > 0;
            if (ok) set_screen(app, MENU_SCREEN_LOAD);
            break;
        case MENU_ITEM_START:
            app->session.difficulty = item->arg;
            ok = start_new_game(app);
            if (ok) close_menu(app);
            break;
        case MENU_ITEM_LOAD:
            ok = load_game_from_file(app, app->saves.entries[item->arg].path);
            if (ok) close_menu(app);
            break;
        case MENU_ITEM_SAVE:
            ok = save_game_to_named_slot(app, app->menu.save_name_input);
            if (ok) set_screen(app, MENU_SCREEN_MAIN);
            break;
        case MENU_ITEM_GOD:
            app->control.god_mode_enabled = !app->control.god_mode_enabled;
            if (app->control.god_mode_enabled) app->player.health = 99;
            break;
        case MENU_ITEM_UNLOCK:
            for (i = 1; i <= 4; ++i) {
                app->player.weapon_slots[i] = 1;
                if (i >= 2) app->player.weapon_ammo[i] = 99;
            }
            break;
        case MENU_ITEM_KEYS:
            for (i = 0; i < ARRAY_COUNT(app->player.keys); ++i) app->player.keys[i] = 1;
            break;
        case MENU_ITEM_BACK:
            go_back(app);
            return;
        case MENU_ITEM_DISPLAY:
        case MENU_ITEM_DEBUG:
            adjust_option(app, item->action, 1);
            return;
        case MENU_ITEM_SENSITIVITY:
        case MENU_ITEM_MUSIC:
        case MENU_ITEM_SOUND:
            return;
    }
    if (ok) {
        play_confirm(app);
    } else {
        play_error(app);
    }
}

/* Keeps the selected save inside the visible window, and the window full. */
static void scroll_to_selection(AppState *app) {
    MenuState *menu = &app->menu;
    if (menu->index < app->saves.count) {
        menu->scroll = bt3d_clamp_i(menu->scroll, menu->index - MENU_VISIBLE_SAVES + 1, menu->index);
    }
    menu->scroll = bt3d_clamp_i(menu->scroll, 0, bt3d_max_i(app->saves.count - MENU_VISIBLE_SAVES, 0));
}

static void delete_save(AppState *app, int save_index) {
    if (!delete_save_file(app, app->saves.entries[save_index].path)) {
        play_error(app);
        return;
    }
    play_confirm(app);
    app->menu.index = bt3d_min_i(app->menu.index, app->saves.count);
    scroll_to_selection(app);
}

static void edit_save_name(MenuState *menu, const FrameInput *input) {
    int i;
    for (i = 0; i < input->typed_count && menu->save_name_length < (int)sizeof(menu->save_name_input) - 1; ++i) {
        menu->save_name_input[menu->save_name_length++] = (char)input->typed[i];
    }
    if (input->erase && menu->save_name_length > 0) menu->save_name_length--;
    menu->save_name_input[menu->save_name_length] = '\0';
}

static float *slider_value(AppState *app, MenuAction action) {
    if (action == MENU_ITEM_MUSIC) return &app->settings.music_volume;
    if (action == MENU_ITEM_SOUND) return &app->settings.sound_volume;
    return NULL;
}

/* Pressing on a slider drags it until the button is released. Music volume
   reaches the stream through bt3d_update_menu_music. */
static void update_slider_drag(AppState *app, const MenuItem *item, const FrameInput *input) {
    float *value = slider_value(app, item->action);
    Rectangle grab;

    if (!value || !input->mouse_down) {
        app->menu.dragging_slider = 0;
        return;
    }
    grab = menu_slider_rect(item->rect);
    grab.y = item->rect.y;
    grab.height = item->rect.height;
    if (input->click && CheckCollisionPointRec(input->mouse, grab)) app->menu.dragging_slider = 1;
    if (app->menu.dragging_slider) *value = bt3d_clamp01((input->mouse.x - grab.x) / grab.width);
}

void bt3d_update_menu(AppState *app, const FrameInput *input) {
    MenuState *menu = &app->menu;
    MenuPage page;
    const MenuItem *item;
    int previous_index = menu->index;
    int navigation = 1;
    int hovered = -1;
    int i;

    if (input->back) {
        go_back(app);
        return;
    }
    if (menu->screen == MENU_SCREEN_SAVE_DIALOG) {
        edit_save_name(menu, input);
        /* Letters and space typed into the name do not also navigate. */
        navigation = input->typed_count == 0;
    }

    menu_build_page(app, &page);
    menu->index = bt3d_min_i(menu->index, page.count - 1);
    if (navigation && input->up) menu->index = (menu->index + page.count - 1) % page.count;
    if (navigation && input->down) menu->index = (menu->index + 1) % page.count;
    for (i = 0; i < page.count; ++i) {
        if (CheckCollisionPointRec(input->mouse, page.items[i].rect)) hovered = i;
    }
    if (hovered >= 0 && !menu->dragging_slider && (input->mouse_moved || input->click)) menu->index = hovered;
    if (menu->screen == MENU_SCREEN_LOAD) scroll_to_selection(app);
    if (menu->index != previous_index) play_move(app);

    item = &page.items[menu->index];
    if (navigation && (input->left || input->right)) adjust_option(app, item->action, input->right ? 1 : -1);
    update_slider_drag(app, item, input);
    if (input->delete_entry && item->action == MENU_ITEM_LOAD) {
        delete_save(app, item->arg);
    } else if ((navigation && input->confirm) || (input->click && hovered == menu->index)) {
        activate(app, item);
    }
}

void bt3d_update_menu_music(AppState *app) {
    MidiPlayer *music = &app->audio.menu_midi;

    if (!app->menu.active) {
        if (music->playing) bt3d_midi_player_stop(music);
        return;
    }
    bt3d_midi_player_set_volume(music, app->settings.music_volume);
    if (!music->playing) {
        bt3d_midi_player_start(music);
    } else {
        bt3d_midi_player_update(music);
    }
}
