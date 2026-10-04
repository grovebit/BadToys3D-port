#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_pickup.h"
#include "bt3d_world_markers.h"

Vector2 marker_world_pos(const MarkerRuntime *marker) {
    return (Vector2){ marker->base.cell_x + 0.5f, marker->base.cell_y + 0.5f };
}

/* Decorations drawn in the top part of their image hang from the ceiling. */
int marker_hangs_from_ceiling(const AppState *app, int marker_type) {
    const PackTexture *patch;
    int top_margin;
    int bottom_margin;

    if (bt3d_is_pickup_marker(marker_type)) return 0;
    patch = bt3d_vec_texture(app, marker_type);
    if (!patch || patch->max_opaque_y < patch->min_opaque_y) return 0;

    top_margin = patch->min_opaque_y;
    bottom_margin = 63 - patch->max_opaque_y;
    return bottom_margin > top_margin + 10 && bottom_margin >= 18;
}

int cell_has_ceiling_marker(const AppState *app, int cell_x, int cell_y) {
    if (!bt3d_cell_in_bounds(cell_x, cell_y)) return 0;
    return app->map_state.ceiling_marker_cell_mask[bt3d_tile_index_for(cell_x, cell_y)];
}
