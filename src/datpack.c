#include "datpack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DATPACK_HEADER_SIZE = 16,
    DATPACK_DIRECTORY_ENTRY_SIZE = 16
};

static const char DATPACK_SIGNATURE[] = "\x07" "DatPack";

static int compare_entry_names(const void *lhs, const void *rhs) {
    return strcmp(((const DatPackEntry *)lhs)->name, ((const DatPackEntry *)rhs)->name);
}

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    unsigned char *data = NULL;
    long size;

    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0) {
        data = (unsigned char *)malloc((size_t)size);
        if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
            free(data);
            data = NULL;
        }
        *out_size = (size_t)size;
    }
    fclose(file);
    return data;
}

int datpack_load(DatPack *pack, const char *path) {
    uint32_t directory_offset;
    uint32_t entry_count;
    size_t i;

    memset(pack, 0, sizeof(*pack));
    pack->data = read_file(path, &pack->size);
    if (!pack->data || pack->size < DATPACK_HEADER_SIZE) goto fail;
    if (memcmp(pack->data, DATPACK_SIGNATURE, sizeof(DATPACK_SIGNATURE) - 1) != 0) goto fail;

    directory_offset = bt3d_le32(&pack->data[8]);
    entry_count = bt3d_le32(&pack->data[12]);
    if (entry_count == 0 || directory_offset > pack->size
        || (pack->size - directory_offset) / DATPACK_DIRECTORY_ENTRY_SIZE < entry_count) goto fail;

    pack->entries = (DatPackEntry *)calloc(entry_count, sizeof(DatPackEntry));
    if (!pack->entries) goto fail;
    for (i = 0; i < entry_count; ++i) {
        const unsigned char *record = &pack->data[directory_offset + i * DATPACK_DIRECTORY_ENTRY_SIZE];
        DatPackEntry *entry = &pack->entries[i];
        size_t name_length = 8;

        memcpy(entry->name, record, 8);
        while (name_length > 0 && (entry->name[name_length - 1] == ' ' || entry->name[name_length - 1] == '\0')) {
            name_length--;
        }
        entry->name[name_length] = '\0';
        entry->offset = bt3d_le32(&record[8]);
        entry->length = bt3d_le32(&record[12]);
        if (entry->offset > pack->size || entry->length > pack->size - entry->offset) goto fail;
    }
    pack->entry_count = entry_count;
    qsort(pack->entries, pack->entry_count, sizeof(DatPackEntry), compare_entry_names);
    return 1;

fail:
    datpack_unload(pack);
    return 0;
}

void datpack_unload(DatPack *pack) {
    free(pack->data);
    free(pack->entries);
    memset(pack, 0, sizeof(*pack));
}

const DatPackEntry *datpack_find(const DatPack *pack, const char *name) {
    size_t lo = 0;
    size_t hi = pack->entry_count;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = strcmp(pack->entries[mid].name, name);
        if (order == 0) return &pack->entries[mid];
        if (order < 0) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}
