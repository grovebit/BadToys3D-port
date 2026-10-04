#ifndef BT3D_DATPACK_H
#define BT3D_DATPACK_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[9];
    uint32_t offset;
    uint32_t length;
} DatPackEntry;

/* The whole archive is kept in memory; entries are sorted by name. */
typedef struct {
    unsigned char *data;
    size_t size;
    DatPackEntry *entries;
    size_t entry_count;
} DatPack;

static inline uint16_t bt3d_le16(const unsigned char *bytes) {
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static inline uint32_t bt3d_le32(const unsigned char *bytes) {
    return (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
}

int datpack_load(DatPack *pack, const char *path);
void datpack_unload(DatPack *pack);
const DatPackEntry *datpack_find(const DatPack *pack, const char *name);

static inline const unsigned char *datpack_entry_data(const DatPack *pack, const DatPackEntry *entry) {
    return pack->data + entry->offset;
}

#endif
