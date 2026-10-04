#include "bt3d_app_state.h"
#include "bt3d_math.h"
#include "bt3d_render.h"
#include "menu_internal.h"

static Color button_text_color(int selected) {
    return selected ? (Color){ 28, 22, 8, 255 } : (Color){ 255, 238, 160, 255 };
}

static void draw_button(Rectangle rect, const char *label, int selected) {
    Color fill = selected ? (Color){ 232, 184, 48, 235 } : (Color){ 174, 124, 30, 220 };
    Color border = selected ? (Color){ 255, 230, 92, 255 } : (Color){ 210, 164, 52, 230 };

    DrawRectangleRounded(rect, 0.08f, 6, fill);
    DrawRectangleLinesEx(rect, 2.0f, border);
    DrawText(label, (int)rect.x + 12, (int)rect.y + 8, 22, button_text_color(selected));
}

static void draw_slider(Rectangle item, float value, int selected) {
    Rectangle slider = menu_slider_rect(item);
    float knob_x = slider.x + slider.width * bt3d_clamp01(value);

    DrawRectangleRounded(slider, 0.6f, 6, (Color){ 64, 45, 20, 210 });
    DrawRectangleRounded((Rectangle){ slider.x, slider.y, knob_x - slider.x, slider.height }, 0.6f, 6, (Color){ 255, 230, 92, 255 });
    DrawCircle((int)knob_x, (int)(slider.y + slider.height * 0.5f), 8.0f, button_text_color(selected));
}

static void draw_name_box(const MenuState *menu, Rectangle box) {
    int empty = menu->save_name_input[0] == '\0';

    DrawRectangleRounded(box, 0.06f, 6, (Color){ 34, 24, 18, 255 });
    DrawRectangleLinesEx(box, 2.0f, (Color){ 92, 70, 38, 255 });
    DrawText(empty ? "save-name" : menu->save_name_input, (int)box.x + 12, (int)box.y + 8, 22, empty ? GRAY : RAYWHITE);
}

void bt3d_draw_menu(const AppState *app) {
    MenuPage page;
    int i;

    menu_build_page(app, &page);
    if (!bt3d_draw_pod_background(app)) ClearBackground((Color){ 10, 8, 7, 255 });
    DrawText(page.title, (int)page.title_position.x, (int)page.title_position.y, 26, RAYWHITE);
    if (app->menu.screen == MENU_SCREEN_SAVE_DIALOG) draw_name_box(&app->menu, page.name_box);

    for (i = 0; i < page.count; ++i) {
        const MenuItem *item = &page.items[i];
        int selected = i == app->menu.index;

        if (item->rect.width <= 0.0f) continue;
        draw_button(item->rect, item->label, selected);
        if (item->action == MENU_ITEM_MUSIC) draw_slider(item->rect, app->settings.music_volume, selected);
        if (item->action == MENU_ITEM_SOUND) draw_slider(item->rect, app->settings.sound_volume, selected);
    }
    if (page.hint) {
        DrawText(page.hint, (int)page.panel.x + 28, (int)(page.panel.y + page.panel.height - 28), 18, LIGHTGRAY);
    }
}
