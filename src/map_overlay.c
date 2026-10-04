#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_world_doors.h"

#include <math.h>

enum {
    MAP_CELL_HIDDEN = 0,
    MAP_CELL_FLOOR,
    MAP_CELL_WALL,
    MAP_CELL_DOOR
};

static const char *map_player_icon_name(float yaw) {
    float angle = yaw;
    while (angle < 0.0f) angle += PI * 2.0f;
    while (angle >= PI * 2.0f) angle -= PI * 2.0f;
    {
        int octant = ((int)floorf((angle + PI / 8.0f) / (PI / 4.0f))) & 7;
        switch (octant) {
            case 0: return "M_HRC_S";
            case 1: return "M_HRC_SV";
            case 2: return "M_HRC_V";
            case 3: return "M_HRC_JV";
            case 4: return "M_HRC_J";
            case 5: return "M_HRC_JZ";
            case 6: return "M_HRC_Z";
            default: return "M_HRC_SZ";
        }
    }
}

/* Door icon frames by slide axis ([1] = x) and how far the door is open. */
static const char *const door_icon_names[2][4] = {
    { "M_DVR_V", "M_DVR_V1", "M_DVR_V2", "M_DVR_V3" },
    { "M_DVR_H", "M_DVR_H1", "M_DVR_H2", "M_DVR_H3" },
};

static int door_icon_stage(float openness) {
    if (openness >= 0.75f) return 3;
    if (openness >= 0.5f) return 2;
    if (openness > 0.0f) return 1;
    return 0;
}

static const Texture2D *bitmap(const AppState *app, const char *name) {
    const PackTexture *texture = bt3d_texture(app, name);
    return texture ? &texture->texture : NULL;
}

static void draw_texture_in(const Texture2D *texture, Rectangle dest) {
    DrawTexturePro(*texture,
        (Rectangle){ 0.0f, 0.0f, (float)texture->width, (float)texture->height },
        dest, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
}

void bt3d_draw_original_map_overlay(AppState *app) {
    const Texture2D *empty_map = bitmap(app, "BM_POD");
    const Texture2D *background = bitmap(app, "M_BGR");
    const Texture2D *wall = bitmap(app, "M_STENA");
    const Texture2D *thing = bitmap(app, "M_VEC");
    const Texture2D *player_icon = bitmap(app, map_player_icon_name(app->player.yaw));
    const Texture2D *door_icons[2][4];
    unsigned char cell_kinds[BT3D_GRID_CELLS];
    float map_size = fminf((float)GetScreenWidth() - 160.0f, (float)GetScreenHeight() - 180.0f);
    float panel_size = fmaxf(320.0f, map_size);
    float cell_size = floorf(panel_size / 64.0f);
    float draw_size = cell_size * 64.0f;
    float origin_x = ((float)GetScreenWidth() - draw_size) * 0.5f;
    float origin_y = ((float)GetScreenHeight() - draw_size) * 0.5f;
    int x, y, i, kind;

    for (i = 0; i < 2; ++i) {
        for (kind = 0; kind < 4; ++kind) {
            door_icons[i][kind] = bitmap(app, door_icon_names[i][kind]);
        }
    }

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 6, 5, 4, 235 });
    DrawRectangleRounded((Rectangle){ origin_x - 12.0f, origin_y - 12.0f, draw_size + 24.0f, draw_size + 24.0f }, 0.02f, 8, (Color){ 20, 16, 12, 245 });
    if (empty_map) {
        draw_texture_in(empty_map, (Rectangle){ origin_x, origin_y, draw_size, draw_size });
    }

    for (i = 0; i < BT3D_GRID_CELLS; ++i) {
        uint16_t tile = app->content.map.tile_grid[i];
        if (!app->map_state.discovered_tile_mask[i]) {
            cell_kinds[i] = MAP_CELL_HIDDEN;
        } else if (map242_is_door(tile)) {
            cell_kinds[i] = MAP_CELL_DOOR;
        } else if (app->map_state.structural_wall_mask[i] || map242_is_solid(tile)) {
            cell_kinds[i] = MAP_CELL_WALL;
        } else {
            cell_kinds[i] = MAP_CELL_FLOOR;
        }
    }

    /* Cells never overlap, so drawing one kind at a time keeps the batch from
       splitting on every texture change without changing the image. */
    for (kind = MAP_CELL_FLOOR; kind <= MAP_CELL_DOOR; ++kind) {
        for (y = 0; y < BT3D_MAP_HEIGHT; ++y) {
            for (x = 0; x < BT3D_MAP_WIDTH; ++x) {
                const Texture2D *tile_texture;
                if (cell_kinds[bt3d_tile_index_for(x, y)] != kind) continue;
                if (kind == MAP_CELL_DOOR) {
                    tile_texture = door_icons[door_slides_on_x_axis(app, x, y)][door_icon_stage(door_openness_at(app, x, y))];
                } else {
                    tile_texture = kind == MAP_CELL_WALL ? wall : background;
                }
                if (tile_texture) {
                    draw_texture_in(tile_texture, (Rectangle){ origin_x + x * cell_size, origin_y + y * cell_size, cell_size, cell_size });
                }
            }
        }
    }

    if (thing) {
        for (i = 0; i < app->entities.marker_count; ++i) {
            MarkerRuntime *marker = &app->entities.markers[i];
            int cell_x = marker->base.cell_x;
            int cell_y = marker->base.cell_y;
            if (marker->collected) continue;
            if (!bt3d_cell_in_bounds(cell_x, cell_y) || !app->map_state.discovered_tile_mask[bt3d_tile_index_for(cell_x, cell_y)]) continue;
            draw_texture_in(thing, (Rectangle){ origin_x + cell_x * cell_size, origin_y + cell_y * cell_size, cell_size, cell_size });
        }
    }

    if (player_icon) {
        float px = origin_x + app->player.position.x * cell_size;
        float py = origin_y + app->player.position.z * cell_size;
        draw_texture_in(player_icon, (Rectangle){ px - cell_size * 0.5f, py - cell_size * 0.5f, cell_size, cell_size });
    }

    DrawText(app->content.map.name, (int)origin_x, (int)(origin_y - 36.0f), 24, RAYWHITE);
    DrawText("M map  Esc menu", (int)origin_x, (int)(origin_y + draw_size + 18.0f), 18, LIGHTGRAY);
}
