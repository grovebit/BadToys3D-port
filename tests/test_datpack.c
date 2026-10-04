#include "datpack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_u32_le(FILE *file, unsigned int value) {
    unsigned char bytes[4];
    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8) & 0xffu);
    bytes[2] = (unsigned char)((value >> 16) & 0xffu);
    bytes[3] = (unsigned char)((value >> 24) & 0xffu);
    fwrite(bytes, 1, sizeof(bytes), file);
}

static int make_temp_path(char *path, size_t path_size) {
    int fd = 0;

    if (!path || path_size < 32) return 0;
    snprintf(path, path_size, "/tmp/bt3d_datpack_test_XXXXXX");
    fd = mkstemp(path);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static int write_valid_pack(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;

    fputc(7, file);
    fwrite("DatPack", 1, 7, file);
    write_u32_le(file, 20);
    write_u32_le(file, 1);
    fwrite("DATA", 1, 4, file);
    fwrite("TEST    ", 1, 8, file);
    write_u32_le(file, 16);
    write_u32_le(file, 4);

    fclose(file);
    return 1;
}

static int write_bad_entry_pack(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;

    fputc(7, file);
    fwrite("DatPack", 1, 7, file);
    write_u32_le(file, 16);
    write_u32_le(file, 1);
    fwrite("BROKEN  ", 1, 8, file);
    write_u32_le(file, 1024);
    write_u32_le(file, 16);

    fclose(file);
    return 1;
}

static int test_valid_pack(void) {
    char path[128];
    DatPack pack;
    const DatPackEntry *entry = NULL;
    int ok = 0;

    memset(&pack, 0, sizeof(pack));
    if (!make_temp_path(path, sizeof(path))) return 0;
    if (!write_valid_pack(path)) goto done;

    if (!datpack_load(&pack, path)) goto done;
    entry = datpack_find(&pack, "TEST");
    if (!entry || entry->length != 4 || memcmp(datpack_entry_data(&pack, entry), "DATA", 4) != 0) goto done;
    ok = 1;

done:
    datpack_unload(&pack);
    unlink(path);
    return ok;
}

static int test_bad_entry_rejected(void) {
    char path[128];
    DatPack pack;
    int ok = 0;

    memset(&pack, 0, sizeof(pack));
    if (!make_temp_path(path, sizeof(path))) return 0;
    if (!write_bad_entry_pack(path)) goto done;
    ok = !datpack_load(&pack, path);

done:
    datpack_unload(&pack);
    unlink(path);
    return ok;
}

int main(void) {
    if (!test_valid_pack()) {
        fprintf(stderr, "valid pack test failed\n");
        return 1;
    }
    if (!test_bad_entry_rejected()) {
        fprintf(stderr, "bad entry pack test failed\n");
        return 1;
    }
    return 0;
}
