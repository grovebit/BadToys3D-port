#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_math.h"
#include "bt3d_render.h"
#include "bt3d_weapon.h"

#include <math.h>

static const Texture2D *bitmap(const AppState *app, const char *name) {
    const PackTexture *texture = bt3d_texture(app, name);
    return texture ? &texture->texture : NULL;
}

static const Texture2D *digit_bitmap(const AppState *app, int digit) {
    char name[] = "BM_N0";
    name[4] = (char)('0' + digit);
    return bitmap(app, name);
}

/* Stretches the whole texture over `dest`. */
static void draw_texture_rect(const Texture2D *texture, Rectangle dest) {
    DrawTexturePro(*texture, (Rectangle){ 0.0f, 0.0f, (float)texture->width, (float)texture->height }, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
}

int bt3d_draw_pod_background(const AppState *app) {
    const Texture2D *background = bitmap(app, "BM_POD");
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    float scale;
    float tile_width;
    float tile_height;
    float start_x;
    float start_y;
    float y;

    if (!background) return 0;

    scale = floorf((float)screen_height / 384.0f);
    if (scale < 1.0f) scale = 1.0f;
    tile_width = (float)background->width * scale;
    tile_height = (float)background->height * scale;
    start_x = fmodf((float)screen_width * 0.5f, tile_width) - tile_width;
    start_y = fmodf((float)screen_height * 0.5f, tile_height) - tile_height;

    ClearBackground(BLACK);
    for (y = start_y; y < (float)screen_height; y += tile_height) {
        float x;
        for (x = start_x; x < (float)screen_width; x += tile_width) {
            DrawTextureEx(*background, (Vector2){ x, y }, 0.0f, scale, WHITE);
        }
    }
    return 1;
}

Rectangle bt3d_hud_rect(const AppState *app) {
    const Texture2D *panel = bitmap(app, "BM_LISTA");
    float screen_width = (float)GetScreenWidth();
    float hud_width = fminf(screen_width, 768.0f);
    float hud_height = 96.0f;
    if (panel && panel->width > 0) {
        hud_height = roundf(((float)panel->height / (float)panel->width) * hud_width);
    }
    return (Rectangle){ floorf((screen_width - hud_width) * 0.5f), 0.0f, hud_width, hud_height };
}

/* Positions and sizes are in the original 256x32 status bar's pixels. */
static void draw_hud_bitmap_at(const Texture2D *texture, Rectangle hud, int source_x, int source_y, int source_width, int source_height) {
    float scale_x = hud.width / 256.0f;
    float scale_y = hud.height / 32.0f;
    if (!texture) return;
    draw_texture_rect(texture, (Rectangle){ hud.x + (float)source_x * scale_x, hud.y + (float)source_y * scale_y,
        (float)source_width * scale_x, (float)source_height * scale_y });
}

/* The lowest `digits` digits of `value`, zero padded. */
static void draw_hud_number(const AppState *app, int value, int digits, Rectangle hud, int source_x, int source_y) {
    int index;
    if (value < 0) value = 0;
    for (index = digits - 1; index >= 0; --index, value /= 10) {
        draw_hud_bitmap_at(digit_bitmap(app, value % 10), hud, source_x + index * 22, source_y, 18, 17);
    }
}

void bt3d_draw_real_hud(const AppState *app) {
    static const int weapon_slot_positions[4][2] = { { 0xdf, 2 }, { 0xf0, 2 }, { 0xdf, 0x11 }, { 0xf0, 0x11 } };
    Rectangle hud = bt3d_hud_rect(app);
    const Texture2D *panel = bitmap(app, "BM_LISTA");
    int slot;

    if (panel) draw_texture_rect(panel, hud);
    if (app->player.current_weapon_slot != 1) {
        draw_hud_number(app, bt3d_player_weapon_current_ammo(app), 2, hud, 7, 2);
    }
    draw_hud_number(app, app->player.health, 2, hud, 0x44, 2);
    draw_hud_number(app, app->player.lives, 1, hud, 0x8b, 2);
    if (app->player.keys[0]) draw_hud_bitmap_at(bitmap(app, "BM_KEY1"), hud, 0xb5, 2, 33, 9);
    if (app->player.keys[1]) draw_hud_bitmap_at(bitmap(app, "BM_KEY2"), hud, 0xb5, 0x0c, 33, 9);
    if (app->player.keys[2]) draw_hud_bitmap_at(bitmap(app, "BM_KEY3"), hud, 0xb5, 0x16, 33, 9);
    for (slot = 1; slot <= 4; ++slot) {
        const char *name = TextFormat("BM_Z%d%s", slot, app->player.weapon_slots[slot] ? "OK" : "NO");
        draw_hud_bitmap_at(bitmap(app, name), hud, weapon_slot_positions[slot - 1][0], weapon_slot_positions[slot - 1][1], 13, 14);
    }
}

void bt3d_draw_level_transition(const AppState *app) {
    const float scale = 4.0f;
    const float gap = 2.0f * scale;
    const Texture2D *label = bitmap(app, "BM_LEVEL");
    const Texture2D *digits[4];
    int level = app->transition.next_index + 1;
    int digit_count = 0;
    float digit_y = (float)GetScreenHeight() * 0.62f;
    float total_width = 0.0f;
    float max_height = 0.0f;
    float x;
    int i;

    if (!bt3d_draw_pod_background(app)) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 6, 5, 4, 255 });
    }
    if (label) {
        float width = label->width * scale;
        float height = label->height * scale;
        draw_texture_rect(label, (Rectangle){ ((float)GetScreenWidth() - width) * 0.5f, 180.0f, width, height });
        digit_y = 180.0f + height + 12.0f;
    }

    /* The level number, most significant digit last. */
    do {
        const Texture2D *digit = digit_bitmap(app, level % 10);
        if (digit) {
            digits[digit_count++] = digit;
            total_width += (float)digit->width * scale + (total_width > 0.0f ? gap : 0.0f);
            max_height = fmaxf(max_height, (float)digit->height * scale);
        }
        level /= 10;
    } while (level > 0 && digit_count < ARRAY_COUNT(digits));

    digit_y = fminf(digit_y, (float)GetScreenHeight() - max_height - 12.0f);
    x = ((float)GetScreenWidth() - total_width) * 0.5f;
    for (i = digit_count - 1; i >= 0; --i) {
        draw_texture_rect(digits[i], (Rectangle){ x, digit_y, digits[i]->width * scale, digits[i]->height * scale });
        x += (float)digits[i]->width * scale + gap;
    }
}

void bt3d_draw_death_transition(const AppState *app) {
    float progress = 1.0f - bt3d_clamp01(app->transition.death_timer / 1.1f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 12, 0, 0, (unsigned char)(140 + progress * 80.0f) });
}

void bt3d_draw_damage_indicator(const AppState *app) {
    float alpha;
    if (app->transition.damage_flash_timer <= 0.0f) return;
    alpha = fminf(1.0f, app->transition.damage_flash_timer / 0.22f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 185, 24, 18, (unsigned char)(alpha * 90.0f) });
}

void bt3d_draw_player_weapon_viewmodel(const AppState *app) {
    const PackTexture *patch = bt3d_vec_texture(app, bt3d_player_weapon_anim_frame(app->player.current_weapon_slot, app->player.attack_timer));
    float scale = fmaxf(3.5f, ((float)GetScreenHeight() / 720.0f) * 8.0f);
    float kick = bt3d_player_weapon_kick(app->player.current_weapon_slot, app->player.attack_timer);
    float width;
    float height;

    if (!patch) return;
    width = patch->texture.width * scale * 0.9f;
    height = patch->texture.height * scale * 1.15f;
    draw_texture_rect(&patch->texture, (Rectangle){ ((float)GetScreenWidth() - width) * 0.5f, (float)GetScreenHeight() - height + kick * 14.0f, width, height });
}
