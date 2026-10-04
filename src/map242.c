#include "map242.h"
#include "bt3d_math.h"
#include "datpack.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAP_HEADER_BLOCK_LENGTH = 13,
    MAP_OPTION_BLOCK_LENGTH = 0x24,
    MAP_MARKER_SIZE = 4,
    MAP_OBJECT_SIZE = 0x28,
    MAP_EVENT_SIZE = 6
};

static const float kPi = 3.14159265358979323846f;

typedef struct {
    const unsigned char *bytes;
    size_t length;
    size_t cursor;
} ByteReader;

static uint8_t reader_u8(ByteReader *reader) {
    if (reader->cursor + 1 > reader->length) {
        return 0;
    }
    return reader->bytes[reader->cursor++];
}

static uint16_t reader_u16(ByteReader *reader) {
    uint16_t value = 0;
    if (reader->cursor + 2 > reader->length) {
        return 0;
    }
    value = bt3d_le16(&reader->bytes[reader->cursor]);
    reader->cursor += 2;
    return value;
}

static uint32_t reader_u32(ByteReader *reader) {
    uint32_t value = 0;
    if (reader->cursor + 4 > reader->length) {
        return 0;
    }
    value = bt3d_le32(&reader->bytes[reader->cursor]);
    reader->cursor += 4;
    return value;
}

/* Everything the header's counts promise, through the descriptor table. The
   counts are 16-bit, so the sum cannot overflow. */
static size_t required_map_length(uint16_t marker_count, uint16_t object_count, uint16_t event_count) {
    const Map242 *map = NULL;
    return MAP_HEADER_BLOCK_LENGTH + 8 + MAP_OPTION_BLOCK_LENGTH + 8
        + sizeof(map->tile_grid) + sizeof(map->detail_grid)
        + (size_t)marker_count * MAP_MARKER_SIZE + (size_t)object_count * MAP_OBJECT_SIZE + (size_t)event_count * MAP_EVENT_SIZE
        + sizeof(map->descriptors);
}

static float heading_to_radians(uint16_t degrees) {
    return -(((float)degrees * kPi) / 180.0f);
}

int map242_is_player_start_object(const MapObject *object) {
    return object->class_id == 0 && object->frame == 0 && object->state == 0;
}

static int find_start_object(const Map242 *map) {
    size_t i = 0;
    for (i = 0; i < map->object_count; ++i) {
        if (map242_is_player_start_object(&map->objects[i])) {
            return (int)i;
        }
    }
    return -1;
}

static int is_simple_walkable_cell(const Map242 *map, int cell_x, int cell_y) {
    if (!map || !bt3d_cell_in_bounds(cell_x, cell_y)) {
        return 0;
    }
    return (map->tile_grid[bt3d_tile_index_for(cell_x, cell_y)] & BT3D_TILE_SOLID_BIT) == 0;
}

static int score_walkable_cell(const Map242 *map, int cell_x, int cell_y) {
    static const int offsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    int score = 0;
    int i = 0;
    for (i = 0; i < 4; ++i) {
        if (is_simple_walkable_cell(map, cell_x + offsets[i][0], cell_y + offsets[i][1])) {
            score++;
        }
    }
    return score;
}

static int find_nearest_walkable_cell(const Map242 *map, float center_x, float center_y, int max_radius, int *out_x, int *out_y) {
    int start_x = bt3d_clamp_i((int)floorf(center_x), 0, BT3D_MAP_WIDTH - 1);
    int start_y = bt3d_clamp_i((int)floorf(center_y), 0, BT3D_MAP_HEIGHT - 1);
    int radius = 0;

    if (is_simple_walkable_cell(map, start_x, start_y)) {
        if (out_x) *out_x = start_x;
        if (out_y) *out_y = start_y;
        return 1;
    }

    for (radius = 1; radius <= max_radius; ++radius) {
        int best_score = -1;
        float best_distance = 1.0e30f;
        int best_x = 0;
        int best_y = 0;
        int found = 0;
        int offset_y = 0;
        int offset_x = 0;

        for (offset_y = -radius; offset_y <= radius; ++offset_y) {
            for (offset_x = -radius; offset_x <= radius; ++offset_x) {
                int cell_x;
                int cell_y;
                int score;
                float dx;
                float dy;
                float distance;

                if (abs(offset_x) != radius && abs(offset_y) != radius) continue;
                cell_x = start_x + offset_x;
                cell_y = start_y + offset_y;
                if (!is_simple_walkable_cell(map, cell_x, cell_y)) continue;

                score = score_walkable_cell(map, cell_x, cell_y);
                dx = ((float)cell_x + 0.5f) - center_x;
                dy = ((float)cell_y + 0.5f) - center_y;
                distance = sqrtf(dx * dx + dy * dy);
                if (score > best_score || (score == best_score && distance < best_distance)) {
                    best_score = score;
                    best_distance = distance;
                    best_x = cell_x;
                    best_y = cell_y;
                    found = 1;
                }
            }
        }

        if (found) {
            if (out_x) *out_x = best_x;
            if (out_y) *out_y = best_y;
            return 1;
        }
    }

    return 0;
}

int map242_parse(Map242 *map, const char *entry_name, const unsigned char *bytes, size_t length) {
    ByteReader reader;
    uint16_t marker_count = 0;
    uint16_t object_count = 0;
    uint16_t event_count = 0;
    size_t i = 0;

    if (!map || !bytes || length < MAP_HEADER_BLOCK_LENGTH + 8) {
        return 0;
    }

    memset(map, 0, sizeof(*map));
    strncpy(map->name, entry_name ? entry_name : "", sizeof(map->name) - 1);

    reader.bytes = bytes;
    reader.length = length;
    reader.cursor = MAP_HEADER_BLOCK_LENGTH;

    marker_count = reader_u16(&reader);
    object_count = reader_u16(&reader);
    event_count = reader_u16(&reader);
    (void)reader_u16(&reader);
    if (length < required_map_length(marker_count, object_count, event_count)) return 0;

    reader.cursor += MAP_OPTION_BLOCK_LENGTH;
    reader.cursor += 4;
    reader.cursor += 2;
    reader.cursor += 2;

    for (i = 0; i < BT3D_GRID_CELLS; ++i) {
        map->tile_grid[i] = reader_u16(&reader);
    }

    for (i = 0; i < sizeof(map->detail_grid); ++i) {
        map->detail_grid[i] = reader_u8(&reader);
    }

    if (marker_count > 0) {
        map->markers = (MapMarker *)calloc(marker_count, sizeof(MapMarker));
        if (!map->markers) {
            map242_unload(map);
            return 0;
        }
    }
    map->marker_count = marker_count;
    for (i = 0; i < map->marker_count; ++i) {
        map->markers[i].cell_x = reader_u8(&reader);
        map->markers[i].cell_y = reader_u8(&reader);
        map->markers[i].type = reader_u16(&reader);
    }

    if (object_count > 0) {
        map->objects = (MapObject *)calloc(object_count, sizeof(MapObject));
        if (!map->objects) {
            map242_unload(map);
            return 0;
        }
    }
    map->object_count = object_count;
    for (i = 0; i < map->object_count; ++i) {
        size_t object_base = reader.cursor;
        map->objects[i].pos_x = (int32_t)reader_u32(&reader);
        map->objects[i].pos_y = (int32_t)reader_u32(&reader);
        map->objects[i].heading_degrees = reader_u16(&reader);
        (void)reader_u16(&reader);
        (void)reader_u16(&reader);
        map->objects[i].class_id = reader_u8(&reader);
        map->objects[i].frame = reader_u8(&reader);
        (void)reader_u8(&reader);
        (void)reader_u8(&reader);
        (void)reader_u8(&reader);
        map->objects[i].state = reader_u8(&reader);
        reader.cursor = object_base + MAP_OBJECT_SIZE;
    }

    if (event_count > 0) {
        map->events = (MapEvent *)calloc(event_count, sizeof(MapEvent));
        if (!map->events) {
            map242_unload(map);
            return 0;
        }
    }
    map->event_count = event_count;
    for (i = 0; i < map->event_count; ++i) {
        map->events[i].type = reader_u8(&reader);
        map->events[i].x = reader_u8(&reader);
        map->events[i].y = reader_u8(&reader);
        map->events[i].state = reader_u8(&reader);
        (void)reader_u16(&reader);
    }

    for (i = 0; i < sizeof(map->descriptors); ++i) {
        map->descriptors[i] = reader_u8(&reader);
    }

    {
        int start_index = find_start_object(map);
        if (start_index >= 0) {
            const MapObject *start = &map->objects[start_index];
            float hint_x = (float)start->pos_x / 65536.0f;
            float hint_y = (float)start->pos_y / 65536.0f;
            int cell_x = 0;
            int cell_y = 0;
            if (find_nearest_walkable_cell(map, hint_x, hint_y, 12, &cell_x, &cell_y)) {
                map->spawn.tile_x = (float)cell_x + 0.5f;
                map->spawn.tile_y = (float)cell_y + 0.5f;
            } else {
                map->spawn.tile_x = hint_x;
                map->spawn.tile_y = hint_y;
            }
            map->spawn.angle_radians = heading_to_radians(start->heading_degrees);
        } else {
            int cell_x = 0;
            int cell_y = 0;
            if (find_nearest_walkable_cell(map, 32.5f, 32.5f, 12, &cell_x, &cell_y)
                || find_nearest_walkable_cell(map, 2.5f, 2.5f, 12, &cell_x, &cell_y)) {
                map->spawn.tile_x = (float)cell_x + 0.5f;
                map->spawn.tile_y = (float)cell_y + 0.5f;
            } else {
                map->spawn.tile_x = 2.5f;
                map->spawn.tile_y = 2.5f;
            }
            map->spawn.angle_radians = 0.0f;
        }
    }

    return 1;
}

void map242_unload(Map242 *map) {
    if (!map) {
        return;
    }
    free(map->markers);
    free(map->objects);
    free(map->events);
    memset(map, 0, sizeof(*map));
}

int map242_is_special_prop_tile(uint16_t tile) {
    return tile == 0x0800
        || tile == 0x0c00
        || tile == 0x0c40
        || tile == 0x4c00
        || tile == 0x4c40
        || tile == 0x8c00
        || tile == 0x8c40
        || tile == 0x8800
        || tile == 0x9c15
        || tile == 0xcc00
        || (tile & 0xfc00) == 0x1c00
        || (tile & 0xfc00) == 0x1800;
}

int map242_is_solid(uint16_t tile) {
    if (tile == 0) {
        return 0;
    }

    /*
     * For MAP_* cells the main blocked/walkable split is the high bit.
     * Doors are the exception: 0x4000-family doors are still blocking even
     * though they do not carry the 0x8000 bit while closed.
     */
    if (map242_is_door(tile)) {
        return 1;
    }
    return (tile & BT3D_TILE_SOLID_BIT) != 0;
}

int map242_is_door(uint16_t tile) {
    uint8_t low = (uint8_t)(tile & 0xff);
    uint16_t family = tile & 0xff00;
    if (map242_is_special_prop_tile(tile)) {
        return 0;
    }
    if (low != 0x40 && low != 0xc0) {
        return 0;
    }
    switch (family) {
        case 0xa800:
        case 0xa900:
        case 0xac00:
        case 0xad00:
        case 0xb000:
        case 0xb100:
        case 0xb400:
        case 0xb500:
        case 0xec00:
        case 0xf400:
        case 0xf500:
            return 1;
        default:
            return 0;
    }
}
