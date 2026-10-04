#include "map242.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TEST_MAP_HEADER_BLOCK_LENGTH = 13,
    TEST_MAP_OPTION_BLOCK_LENGTH = 0x24
};

static unsigned char *make_minimal_map(size_t *out_size) {
    size_t size = TEST_MAP_HEADER_BLOCK_LENGTH + 8 + TEST_MAP_OPTION_BLOCK_LENGTH + 4 + 2 + 2
        + BT3D_GRID_CELLS * 2
        + BT3D_GRID_CELLS * 4
        + 255 * 3;
    /* Zeroed header counts describe an empty map. */
    unsigned char *bytes = (unsigned char *)calloc(size, 1);

    if (!bytes) return NULL;
    if (out_size) *out_size = size;
    return bytes;
}

static int test_minimal_map_parses(void) {
    Map242 map;
    unsigned char *bytes = NULL;
    size_t size = 0;
    int ok = 0;

    memset(&map, 0, sizeof(map));
    bytes = make_minimal_map(&size);
    if (!bytes) return 0;
    ok = map242_parse(&map, "MAP_TEST", bytes, size)
        && map.marker_count == 0
        && map.object_count == 0
        && map.event_count == 0;
    map242_unload(&map);
    free(bytes);
    return ok;
}

static int test_truncated_map_rejected(void) {
    Map242 map;
    unsigned char *bytes = NULL;
    size_t size = 0;
    int ok = 0;

    memset(&map, 0, sizeof(map));
    bytes = make_minimal_map(&size);
    if (!bytes || size < 16) {
        free(bytes);
        return 0;
    }
    ok = !map242_parse(&map, "MAP_BAD", bytes, size - 16);
    map242_unload(&map);
    free(bytes);
    return ok;
}

int main(void) {
    if (!test_minimal_map_parses()) {
        fprintf(stderr, "minimal map parse test failed\n");
        return 1;
    }
    if (!test_truncated_map_rejected()) {
        fprintf(stderr, "truncated map rejection test failed\n");
        return 1;
    }
    return 0;
}
