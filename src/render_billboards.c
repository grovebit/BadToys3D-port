#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"
#include "bt3d_render.h"
#include "bt3d_world_markers.h"
#include "bt3d_world_overlays.h"
#include "profiler.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    const Texture2D *texture;
    Rectangle source;
    Vector3 position;
    Vector2 size;
    float distance_sq;
} BillboardSprite;

enum { MAX_BILLBOARDS = 2048 };

/* Per-frame scratch, rebuilt every frame. */
static BillboardSprite sprites[MAX_BILLBOARDS];
static int sprite_count;

static int compare_sprites_far_first(const void *lhs, const void *rhs) {
    float a = ((const BillboardSprite *)lhs)->distance_sq;
    float b = ((const BillboardSprite *)rhs)->distance_sq;
    return (a < b) - (a > b);
}

static BillboardSprite *add_sprite(const Texture2D *texture, Vector3 position, float width, float height, Vector3 camera_position) {
    BillboardSprite *sprite;
    if (sprite_count >= MAX_BILLBOARDS) return NULL;
    sprite = &sprites[sprite_count++];
    sprite->texture = texture;
    sprite->source = (Rectangle){ 0.0f, 0.0f, (float)texture->width, (float)texture->height };
    sprite->position = position;
    sprite->size = (Vector2){ width, height };
    sprite->distance_sq = Vector3DistanceSqr(position, camera_position);
    return sprite;
}

/* Crops the sprite to its opaque texels from `crop_top_y` down and stands it
   on the floor, keeping each texel where it was in the full image. */
static void crop_sprite_to_bounds(BillboardSprite *sprite, const PackTexture *patch, int crop_top_y, Vector3 camera_right) {
    float texture_width = (float)patch->texture.width;
    float texture_height = (float)patch->texture.height;
    float size = sprite->size.x;
    int min_x = patch->min_opaque_x;
    int max_x = patch->max_opaque_x;
    int min_y = bt3d_max_i(crop_top_y, patch->min_opaque_y);
    int max_y = patch->max_opaque_y;
    float source_width;
    float source_height;
    float x_center;
    float y_center;

    if (min_x < 0 || max_x < min_x || min_y < 0 || max_y < min_y) return;
    source_width = (float)(max_x - min_x + 1);
    source_height = (float)(max_y - min_y + 1);
    x_center = ((float)min_x + source_width * 0.5f) - texture_width * 0.5f;
    y_center = (float)min_y + source_height * 0.5f;
    sprite->position.x += camera_right.x * (x_center / texture_width) * size;
    sprite->position.z += camera_right.z * (x_center / texture_width) * size;
    sprite->position.y = ((texture_height - y_center) / texture_height) * size;
    sprite->size = (Vector2){ size * (source_width / texture_width), size * (source_height / texture_height) };
    sprite->source = (Rectangle){ (float)min_x, (float)min_y, source_width, source_height };
}

static void draw_sprite_quad(const BillboardSprite *sprite, Vector3 camera_right) {
    float tex_w = (float)sprite->texture->width;
    float tex_h = (float)sprite->texture->height;
    float u0 = sprite->source.x / tex_w;
    float v0 = sprite->source.y / tex_h;
    float u1 = (sprite->source.x + sprite->source.width) / tex_w;
    float v1 = (sprite->source.y + sprite->source.height) / tex_h;
    Vector3 right = Vector3Scale(camera_right, sprite->size.x * 0.5f);
    Vector3 up = { 0.0f, sprite->size.y * 0.5f, 0.0f };
    Vector3 p0 = Vector3Subtract(Vector3Subtract(sprite->position, right), up);
    Vector3 p1 = Vector3Add(Vector3Subtract(sprite->position, up), right);
    Vector3 p2 = Vector3Add(Vector3Add(sprite->position, right), up);
    Vector3 p3 = Vector3Add(Vector3Subtract(sprite->position, right), up);

    rlSetTexture(sprite->texture->id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlCheckRenderBatchLimit(4);
    rlTexCoord2f(u0, v1); rlVertex3f(p0.x, p0.y, p0.z);
    rlTexCoord2f(u1, v1); rlVertex3f(p1.x, p1.y, p1.z);
    rlTexCoord2f(u1, v0); rlVertex3f(p2.x, p2.y, p2.z);
    rlTexCoord2f(u0, v0); rlVertex3f(p3.x, p3.y, p3.z);
    rlEnd();
}

/* Things outside the render mask or the view are skipped; positions outside the map never are. */
static int sprite_hidden(const AppState *app, const Bt3dViewCone *view, float x, float z) {
    int cell_x = (int)floorf(x);
    int cell_y = (int)floorf(z);
    if (bt3d_cell_in_bounds(cell_x, cell_y) && !app->map_state.render_tile_mask[bt3d_tile_index_for(cell_x, cell_y)]) return 1;
    return !bt3d_view_cone_contains(view, x, z, 0.75f);
}

static void gather_markers(const AppState *app, const Bt3dViewCone *view, Vector3 camera_position) {
    int i;

    for (i = 0; i < app->entities.marker_count; ++i) {
        const MarkerRuntime *marker = &app->entities.markers[i];
        Vector2 pos = marker_world_pos(marker);
        const PackTexture *patch;
        float center_y = MARKER_BILLBOARD_SIZE * 0.5f;

        if (marker->collected || sprite_hidden(app, view, pos.x, pos.y)) continue;
        patch = bt3d_vec_texture(app, bt3d_animated_overlay_id(app, marker->base.type));
        if (!patch) continue;
        if (marker_hangs_from_ceiling(app, marker->base.type)) center_y = WALL_HEIGHT - center_y - 0.04f;
        add_sprite(&patch->texture, (Vector3){ pos.x, center_y, pos.y }, MARKER_BILLBOARD_SIZE, MARKER_BILLBOARD_SIZE, camera_position);
    }
}

static void gather_enemies(const AppState *app, const Bt3dViewCone *view, Vector3 camera_position, Vector3 camera_right) {
    int i;

    for (i = 0; i < app->entities.enemy_count; ++i) {
        const EnemyRuntime *enemy = &app->entities.enemies[i];
        const PackTexture *patch;
        BillboardSprite *sprite;
        char entry_name[16];

        if (!enemy->alive && enemy->anim_state != ENEMY_ANIM_DIE) continue;
        if (sprite_hidden(app, view, enemy->position.x, enemy->position.z)) continue;
        bt3d_sprite_entry_name_for_class(enemy->class_id, enemy->frame, entry_name, sizeof(entry_name));
        patch = bt3d_texture(app, entry_name);
        if (!patch) continue;
        sprite = add_sprite(&patch->texture, (Vector3){ enemy->position.x, ENEMY_BILLBOARD_SIZE * 0.5f, enemy->position.z },
            ENEMY_BILLBOARD_SIZE, ENEMY_BILLBOARD_SIZE, camera_position);
        if (!sprite) continue;
        /* Corpses crop off stray pixels above the body and sit slightly higher. */
        crop_sprite_to_bounds(sprite, patch, enemy->alive ? patch->min_opaque_y : patch->visual_min_opaque_y, camera_right);
        if (!enemy->alive) sprite->position.y += 0.03f;
    }
}

static void gather_projectiles_and_impacts(const AppState *app, const Bt3dViewCone *view, Vector3 camera_position) {
    char entry_name[16];
    int i;

    for (i = 0; i < app->projectiles.enemy_count; ++i) {
        const EnemyProjectile *projectile = &app->projectiles.enemy[i];
        const PackTexture *patch;

        if (sprite_hidden(app, view, projectile->position.x, projectile->position.z)) continue;
        bt3d_sprite_entry_name_for_class(projectile->projectile_class_id,
            bt3d_enemy_projectile_frame(projectile->projectile_class_id, projectile->animation_timer), entry_name, sizeof(entry_name));
        patch = bt3d_texture(app, entry_name);
        if (patch) add_sprite(&patch->texture, (Vector3){ projectile->position.x, 0.45f, projectile->position.z }, 0.9f, 0.9f, camera_position);
    }

    for (i = 0; i < app->projectiles.shot_impact_count; ++i) {
        const PlayerShotImpact *impact = &app->projectiles.shot_impacts[i];
        float progress = impact->life > 0.0f ? fminf(1.0f, impact->age / impact->life) : 1.0f;
        const PackTexture *patch;

        bt3d_sprite_entry_name_for_class(BT3D_SPRITE_CLASS_SHOT_IMPACT, bt3d_min_i((int)floorf(progress * 3.0f), 2), entry_name, sizeof(entry_name));
        patch = bt3d_texture(app, entry_name);
        if (patch) add_sprite(&patch->texture, impact->position, 0.38f, 0.38f, camera_position);
    }
}

/* Sprites are blended far to near and leave the depth buffer alone. */
void bt3d_draw_billboard_sprites(AppState *app, Camera3D camera) {
    Bt3dViewCone view = bt3d_view_cone(camera);
    Vector3 camera_forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 camera_right = Vector3Normalize(Vector3CrossProduct(camera_forward, camera.up));
    int i;

    BT3D_PROF_BEGIN("db_gather");
    sprite_count = 0;
    gather_markers(app, &view, camera.position);
    gather_enemies(app, &view, camera.position, camera_right);
    gather_projectiles_and_impacts(app, &view, camera.position);
    qsort(sprites, (size_t)sprite_count, sizeof(BillboardSprite), compare_sprites_far_first);
    BT3D_PROF_END("db_gather");

    BT3D_PROF_BEGIN("db_draw");
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    for (i = 0; i < sprite_count; ++i) {
        draw_sprite_quad(&sprites[i], camera_right);
    }
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    BT3D_PROF_END("db_draw");
}
