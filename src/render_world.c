#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_doors.h"
#include "bt3d_math.h"
#include "bt3d_render.h"
#include "bt3d_world_doors.h"
#include "bt3d_world_overlays.h"
#include "bt3d_world_textures.h"
#include "profiler.h"

#include "rlgl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* World geometry is gathered into textured quads every frame and drawn grouped
   by texture: opaque surfaces first, then the overlays (blended, without depth
   writes) once the surfaces they sit on are in the depth buffer. */

typedef struct {
    float x, y, z, u, v;
} QuadVertex;

/* Vertices run counter-clockwise as seen from the visible side. */
typedef struct {
    int texture;
    QuadVertex v[4];
} WorldQuad;

typedef struct {
    WorldQuad *quads;
    int count;
    int capacity;
} QuadList;

/* Per-frame scratch, rebuilt every frame; nothing here outlives one. */
static WorldQuad opaque_storage[BT3D_GRID_CELLS * 4];
static WorldQuad overlay_storage[BT3D_GRID_CELLS * 4];
static QuadList opaque_quads = { opaque_storage, 0, BT3D_GRID_CELLS * 4 };
static QuadList overlay_quads = { overlay_storage, 0, BT3D_GRID_CELLS * 4 };

static const float DOOR_THICKNESS = 0.18f;

static void push_quad(QuadList *list, const AppState *app, const PackTexture *texture, const QuadVertex v[4]) {
    WorldQuad *quad;
    if (!texture || list->count >= list->capacity) return;
    quad = &list->quads[list->count++];
    quad->texture = (int)(texture - app->assets.textures);
    memcpy(quad->v, v, sizeof(quad->v));
}

/* raylib's unit plane mesh: (x, y) offset and (u, v) of each vertex. Its
   counter-clockwise side faces away from its normal. */
static const float plane_corners[4][4] = {
    { -0.5f, -0.5f, 0.0f, 1.0f },
    { -0.5f, 0.5f, 0.0f, 0.0f },
    { 0.5f, 0.5f, 1.0f, 0.0f },
    { 0.5f, -0.5f, 1.0f, 1.0f },
};

/* Pushes the plane scaled to width x height, turned `quarter_turns` x 90
   degrees about +Y and centred on `center`; its texture coordinates are
   mapped onto u0..u1 and v0..v1. */
static void push_plane(QuadList *list, const AppState *app, const PackTexture *texture, Vector3 center,
                       int quarter_turns, float width, float height, float u0, float u1, float v0, float v1) {
    /* Where the plane's local +x axis points, as (x, z). */
    static const Vector2 axes[4] = { { 1.0f, 0.0f }, { 0.0f, -1.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f } };
    Vector2 axis = axes[quarter_turns & 3];
    QuadVertex v[4];
    int i;

    for (i = 0; i < 4; ++i) {
        float along = plane_corners[i][0] * width;
        v[i].x = center.x + axis.x * along;
        v[i].y = center.y + plane_corners[i][1] * height;
        v[i].z = center.z + axis.y * along;
        v[i].u = u0 + plane_corners[i][2] * (u1 - u0);
        v[i].v = v0 + plane_corners[i][3] * (v1 - v0);
    }
    push_quad(list, app, texture, v);
}

/* The four side faces of raylib's unit cube mesh as (x, y, z) offset and (u, v). */
static const float cube_sides[4][4][5] = {
    { { -0.5f, -0.5f, 0.5f, 0, 0 }, { 0.5f, -0.5f, 0.5f, 1, 0 }, { 0.5f, 0.5f, 0.5f, 1, 1 }, { -0.5f, 0.5f, 0.5f, 0, 1 } },
    { { -0.5f, -0.5f, -0.5f, 1, 0 }, { -0.5f, 0.5f, -0.5f, 1, 1 }, { 0.5f, 0.5f, -0.5f, 0, 1 }, { 0.5f, -0.5f, -0.5f, 0, 0 } },
    { { 0.5f, -0.5f, -0.5f, 1, 0 }, { 0.5f, 0.5f, -0.5f, 1, 1 }, { 0.5f, 0.5f, 0.5f, 0, 1 }, { 0.5f, -0.5f, 0.5f, 0, 0 } },
    { { -0.5f, -0.5f, -0.5f, 0, 0 }, { -0.5f, -0.5f, 0.5f, 1, 0 }, { -0.5f, 0.5f, 0.5f, 1, 1 }, { -0.5f, 0.5f, -0.5f, 0, 1 } },
};

/* Top and bottom are left out: they lie in the ceiling and floor planes. */
static void push_box_sides(QuadList *list, const AppState *app, const PackTexture *texture, Vector3 center, Vector3 size) {
    int side;
    int i;

    for (side = 0; side < 4; ++side) {
        QuadVertex v[4];
        for (i = 0; i < 4; ++i) {
            const float *corner = cube_sides[side][i];
            v[i] = (QuadVertex){ center.x + corner[0] * size.x, center.y + corner[1] * size.y, center.z + corner[2] * size.z, corner[3], corner[4] };
        }
        push_quad(list, app, texture, v);
    }
}

static void push_floor_and_ceiling(const AppState *app, int x, int y, int floor_texture, int ceiling_texture) {
    float x0 = (float)x, z0 = (float)y, x1 = x0 + 1.0f, z1 = z0 + 1.0f;
    const QuadVertex floor[4] = {
        { x0, 0.0f, z0, 0.0f, 1.0f }, { x0, 0.0f, z1, 0.0f, 0.0f }, { x1, 0.0f, z1, 1.0f, 0.0f }, { x1, 0.0f, z0, 1.0f, 1.0f },
    };
    const QuadVertex ceiling[4] = {
        { x0, WALL_HEIGHT, z0, 1.0f, 1.0f }, { x1, WALL_HEIGHT, z0, 0.0f, 1.0f }, { x1, WALL_HEIGHT, z1, 0.0f, 0.0f }, { x0, WALL_HEIGHT, z1, 1.0f, 0.0f },
    };
    push_quad(&opaque_quads, app, bt3d_wall_texture(app, floor_texture), floor);
    push_quad(&opaque_quads, app, bt3d_wall_texture(app, ceiling_texture), ceiling);
}

/* Wall face f of a cell faces west, east, south (+z) and north (-z). */
static const struct {
    int dx, dy;
    int quarter_turns;
} wall_faces[4] = {
    { -1, 0, 1 }, { 1, 0, 3 }, { 0, 1, 2 }, { 0, -1, 0 },
};

static Vector3 wall_face_center(int x, int y, int face, float outward) {
    return (Vector3){
        x + 0.5f + wall_faces[face].dx * (0.5f + outward),
        WALL_HEIGHT * 0.5f,
        y + 0.5f + wall_faces[face].dy * (0.5f + outward),
    };
}

static void push_structural_wall(const AppState *app, int x, int y) {
    int face;

    for (face = 0; face < 4; ++face) {
        int nx = x + wall_faces[face].dx;
        int ny = y + wall_faces[face].dy;
        int overlay_id;

        if (bt3d_cell_in_bounds(nx, ny) && app->map_state.structural_wall_mask[bt3d_tile_index_for(nx, ny)]) continue;
        push_plane(&opaque_quads, app, bt3d_wall_texture(app, wall_texture_index_for_face(app, x, y, face)),
            wall_face_center(x, y, face, 0.0f), wall_faces[face].quarter_turns, 1.0f, WALL_HEIGHT, 1.0f, 0.0f, 1.0f, 0.0f);

        /* Overlays sit 1mm in front of the wall, mirrored to read correctly. */
        overlay_id = wall_overlay_index_for_face(app, x, y, face);
        if (overlay_id > 0) {
            push_plane(&overlay_quads, app, bt3d_vec_texture(app, bt3d_animated_overlay_id(app, overlay_id)),
                wall_face_center(x, y, face, 0.001f), wall_faces[face].quarter_turns, 1.0f, WALL_HEIGHT, 1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}

/* Centre-split doors part in the middle; the others slide along their axis
   as one leaf. The cell's first wall texture is the door's face. */
static void push_door(const AppState *app, uint16_t tile, int x, int y) {
    static const float overlay_height = 0.84f;
    /* Half the leaf thickness plus 1mm keeps overlays just in front of the leaf. */
    const float overlay_offset = DOOR_THICKNESS * 0.5f + 0.001f;
    int slides_on_x_axis = door_slides_on_x_axis(app, x, y);
    int center_split = bt3d_is_center_split_door_tile(tile);
    float slide = door_openness_at(app, x, y);
    float slide_sign = ((tile & 0xff) == 0x40) ? 1.0f : -1.0f;
    const PackTexture *texture = bt3d_wall_texture(app, wall_texture_index_for_face(app, x, y, 0));
    int overlay_id = door_overlay_index_for_cell(app, x, y);
    const PackTexture *overlay = overlay_id > 0 ? bt3d_vec_texture(app, bt3d_animated_overlay_id(app, overlay_id)) : NULL;
    int turns = slides_on_x_axis ? 0 : 1;
    Vector2 along = slides_on_x_axis ? (Vector2){ 1.0f, 0.0f } : (Vector2){ 0.0f, 1.0f };
    Vector2 across = { along.y, along.x };
    int leaf;

    if (slide >= 0.999f) return;

    for (leaf = 0; leaf < (center_split ? 2 : 1); ++leaf) {
        float leaf_offset = center_split ? (leaf == 0 ? -0.25f : 0.25f) : 0.0f;
        float shift = center_split ? leaf_offset * (1.0f + slide * 2.0f) : slide * slide_sign;
        Vector3 center = { x + 0.5f + along.x * shift, WALL_HEIGHT * 0.5f, y + 0.5f + along.y * shift };
        float width = center_split ? 0.5f : 1.0f;
        /* Texture u range of this leaf: the half on its side for split doors. */
        float u0 = 0.0f;
        float u1 = 1.0f;
        float side;

        if (center_split) {
            u0 = 0.5f * (float)bt3d_split_door_slice_index(tile, slides_on_x_axis, leaf_offset);
            u1 = u0 + 0.5f;
            /* An outward-facing plane on each side of the leaf; the second is
               mirrored so the image reads the same from both sides. */
            push_plane(&opaque_quads, app, texture,
                (Vector3){ center.x - across.x * DOOR_THICKNESS * 0.5f, center.y, center.z - across.y * DOOR_THICKNESS * 0.5f },
                turns, width, WALL_HEIGHT, u0, u1, 0.0f, 1.0f);
            push_plane(&opaque_quads, app, texture,
                (Vector3){ center.x + across.x * DOOR_THICKNESS * 0.5f, center.y, center.z + across.y * DOOR_THICKNESS * 0.5f },
                turns + 2, width, WALL_HEIGHT, u1, u0, 0.0f, 1.0f);
        } else {
            Vector3 size = slides_on_x_axis ? (Vector3){ width, WALL_HEIGHT, DOOR_THICKNESS } : (Vector3){ DOOR_THICKNESS, WALL_HEIGHT, width };
            push_box_sides(&opaque_quads, app, texture, center, size);
        }

        if (!overlay) continue;
        for (side = -1.0f; side <= 1.0f; side += 2.0f) {
            Vector3 overlay_center = {
                center.x + across.x * overlay_offset * side,
                0.48f,
                center.z + across.y * overlay_offset * side,
            };
            push_plane(&overlay_quads, app, overlay, overlay_center, side > 0.0f ? turns : turns + 2,
                center_split ? 0.45f : 0.9f, overlay_height, u0, u1, 0.0f, 1.0f);
        }
    }
}

static void gather_world_quads(const AppState *app, const Bt3dViewCone *view) {
    int i;

    opaque_quads.count = 0;
    overlay_quads.count = 0;
    for (i = 0; i < app->map_state.render_cell_count; ++i) {
        int index = app->map_state.render_cell_indices[i];
        int x = index / BT3D_MAP_WIDTH;
        int y = index % BT3D_MAP_WIDTH;
        uint16_t tile = app->content.map.tile_grid[index];
        int door = map242_is_door(tile);

        /* Door leaves slide up to a cell away from their own. */
        if (!bt3d_view_cone_contains(view, x + 0.5f, y + 0.5f, door ? 1.6f : 0.75f)) continue;
        if (app->map_state.structural_wall_mask[index]) {
            push_structural_wall(app, x, y);
            continue;
        }
        if (app->map_state.event_index_by_cell[index] >= 0) {
            push_floor_and_ceiling(app, x, y, floor_texture_index_for_cell(app, x, y), ceiling_texture_index_for_cell(app, x, y));
        } else {
            push_floor_and_ceiling(app, x, y, app->map_state.raw_floor_tex_cache[index], app->map_state.raw_ceil_tex_cache[index]);
        }
        if (door) push_door(app, tile, x, y);
    }
}

/* Draws the quads grouped by texture (a counting sort on the texture index). */
static void draw_quads(const AppState *app, const QuadList *list) {
    static int order[BT3D_GRID_CELLS * 4];
    static int *starts;
    static int starts_capacity;
    int texture_count = (int)app->content.pack.entry_count;
    int current = -1;
    int i;
    int k;

    if (list->count == 0) return;
    if (starts_capacity < texture_count + 1) {
        int *grown = (int *)realloc(starts, sizeof(int) * (size_t)(texture_count + 1));
        if (!grown) return;
        starts = grown;
        starts_capacity = texture_count + 1;
    }
    memset(starts, 0, sizeof(int) * (size_t)(texture_count + 1));
    for (i = 0; i < list->count; ++i) starts[list->quads[i].texture + 1]++;
    for (i = 0; i < texture_count; ++i) starts[i + 1] += starts[i];
    for (i = 0; i < list->count; ++i) order[starts[list->quads[i].texture]++] = i;

    for (i = 0; i < list->count; ++i) {
        const WorldQuad *quad = &list->quads[order[i]];
        if (quad->texture != current) {
            if (current >= 0) rlEnd();
            current = quad->texture;
            rlSetTexture(app->assets.textures[current].texture.id);
            rlBegin(RL_QUADS);
            rlColor4ub(255, 255, 255, 255);
        }
        /* raylib-nx drops vertices past a full batch instead of flushing it. */
        rlCheckRenderBatchLimit(4);
        for (k = 0; k < 4; ++k) {
            rlTexCoord2f(quad->v[k].u, quad->v[k].v);
            rlVertex3f(quad->v[k].x, quad->v[k].y, quad->v[k].z);
        }
    }
    rlEnd();
    rlSetTexture(0);
}

Bt3dViewCone bt3d_view_cone(Camera3D camera) {
    Bt3dViewCone cone = { 0 };
    Vector2 forward = { camera.target.x - camera.position.x, camera.target.z - camera.position.z };
    float length = sqrtf(forward.x * forward.x + forward.y * forward.y);
    float aspect = (float)rlGetFramebufferWidth() / (float)bt3d_max_i(1, rlGetFramebufferHeight());
    float half_angle = atanf(tanf(camera.fovy * DEG2RAD * 0.5f) * aspect) + 0.05f;
    Vector2 right;

    cone.origin = (Vector2){ camera.position.x, camera.position.z };
    if (length < 1.0e-6f || half_angle > 1.5f) return cone;
    forward.x /= length;
    forward.y /= length;
    right = (Vector2){ forward.y, -forward.x };
    cone.edge_normals[0] = (Vector2){ forward.x * sinf(half_angle) + right.x * cosf(half_angle), forward.y * sinf(half_angle) + right.y * cosf(half_angle) };
    cone.edge_normals[1] = (Vector2){ forward.x * sinf(half_angle) - right.x * cosf(half_angle), forward.y * sinf(half_angle) - right.y * cosf(half_angle) };
    cone.culling = 1;
    return cone;
}

int bt3d_view_cone_contains(const Bt3dViewCone *cone, float x, float z, float radius) {
    float dx = x - cone->origin.x;
    float dz = z - cone->origin.y;
    if (!cone->culling) return 1;
    return dx * cone->edge_normals[0].x + dz * cone->edge_normals[0].y >= -radius
        && dx * cone->edge_normals[1].x + dz * cone->edge_normals[1].y >= -radius;
}

void bt3d_draw_map_world(AppState *app, Camera3D camera) {
    Bt3dViewCone view = bt3d_view_cone(camera);
    int i;

    BT3D_PROF_BEGIN("dw_gather");
    gather_world_quads(app, &view);
    BT3D_PROF_END("dw_gather");

    BT3D_PROF_BEGIN("dw_draw");
    draw_quads(app, &opaque_quads);
    for (i = 0; i < app->projectiles.player_count; ++i) {
        DrawSphereEx(app->projectiles.player[i].position, 0.12f, 6, 8, (Color){ 255, 180, 64, 255 });
    }
    /* State changes only reach geometry already flushed to the GPU. */
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    draw_quads(app, &overlay_quads);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    BT3D_PROF_END("dw_draw");
}
