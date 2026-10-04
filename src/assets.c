#include "bt3d_app_state.h"
#include "bt3d_assets.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"

#include <stdlib.h>
#include <string.h>

/* Allocated with MemAlloc so UnloadImage can free the pixels. */
static Image new_rgba_image(int width, int height) {
    Image image = { 0 };
    image.data = MemAlloc((unsigned int)(width * height) * sizeof(Color));
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return image;
}

/* STN_<id> holds 64x64 palette indices stored a quarter turn clockwise. */
static Image decode_stn(const Color *palette, const unsigned char *bytes, size_t size) {
    Image image = { 0 };
    Color *pixels;
    int i;

    if (size != 64 * 64) return image;
    image = new_rgba_image(64, 64);
    pixels = (Color *)image.data;
    for (i = 0; i < 64 * 64; ++i) {
        pixels[(63 - i % 64) * 64 + i / 64] = palette[bytes[i]];
    }
    return image;
}

/* Column-run patch (sprite or overlay) decoded into a 64x64 image. */
static Image decode_patch(const Color *palette, const unsigned char *bytes, size_t size, PackTexture *out_bounds) {
    Image image = { 0 };
    Color *pixels;
    int row_opaque_counts[64] = { 0 };
    int column;
    int row;

    if (size < 0x100) return image;
    image = new_rgba_image(64, 64);
    pixels = (Color *)image.data;
    memset(pixels, 0, sizeof(Color) * 64 * 64);
    out_bounds->min_opaque_x = 64;
    out_bounds->max_opaque_x = -1;
    out_bounds->min_opaque_y = 64;
    out_bounds->max_opaque_y = -1;
    out_bounds->visual_min_opaque_y = 64;

    for (column = 0; column < 64; ++column) {
        int offset = bt3d_le16(&bytes[column * 2]);
        int start_y = bytes[0x80 + column * 2];
        int run_length = bytes[0x80 + column * 2 + 1];
        int y;
        for (y = 0; y < run_length; ++y) {
            int target_y = start_y + y;
            unsigned char palette_index;
            if (target_y >= 64 || offset + y >= (int)size) continue;
            palette_index = bytes[offset + y];
            if (palette_index == 255) continue;
            pixels[target_y * 64 + column] = palette[palette_index];
            row_opaque_counts[target_y]++;
            if (column < out_bounds->min_opaque_x) out_bounds->min_opaque_x = column;
            if (column > out_bounds->max_opaque_x) out_bounds->max_opaque_x = column;
            if (target_y < out_bounds->min_opaque_y) out_bounds->min_opaque_y = target_y;
            if (target_y > out_bounds->max_opaque_y) out_bounds->max_opaque_y = target_y;
        }
    }

    /* The first row with enough opaque texels below it, ignoring stray
       pixels such as antennae when sitting a sprite on the floor. */
    for (row = out_bounds->min_opaque_y; row <= out_bounds->max_opaque_y; ++row) {
        int support = row_opaque_counts[row];
        if (row + 1 <= out_bounds->max_opaque_y) support += row_opaque_counts[row + 1];
        if (row + 2 <= out_bounds->max_opaque_y) support += row_opaque_counts[row + 2];
        if (row + 3 <= out_bounds->max_opaque_y) support += row_opaque_counts[row + 3];
        if (row_opaque_counts[row] >= 4 && support >= 12) {
            out_bounds->visual_min_opaque_y = row;
            break;
        }
    }
    if (out_bounds->visual_min_opaque_y == 64) out_bounds->visual_min_opaque_y = out_bounds->min_opaque_y;
    return image;
}

/* Blue-dominant colours are the bitmaps' transparency key. */
static Color apply_bitmap_color_key(Color c) {
    if ((c.r == 0 && c.g == 0 && c.b == 100)
        || (c.b > 40 && (float)c.b >= (float)c.r * 1.8f && (float)c.b >= (float)c.g * 1.8f)) {
        c.a = 0;
    }
    return c;
}

/* Writes the RLE cursor's pixel and advances it; returns 0 once past the last row. */
static int bitmap_put_next_pixel(Image *image, int top_down, int *x, int *y, Color c) {
    if (*x < image->width && *y < image->height) {
        int dst_y = top_down ? *y : (image->height - 1 - *y);
        ((Color *)image->data)[dst_y * image->width + *x] = c;
    }
    *x += 1;
    if (*x >= image->width) {
        *x = 0;
        *y += 1;
    }
    return *y < image->height;
}

/* 4/8-bit Windows BMP, uncompressed or RLE. */
static Image decode_bitmap(const unsigned char *bytes, size_t size) {
    Image image = { 0 };
    Color palette[256];
    int pixel_offset, dib_size, width, height, top_down, bits_per_pixel, compression;
    int palette_entries, palette_offset, row_bytes, byte_index, i;

    if (size < 54 || bytes[0] != 'B' || bytes[1] != 'M') return image;
    pixel_offset = (int)bt3d_le32(&bytes[10]);
    dib_size = (int)bt3d_le32(&bytes[14]);
    width = (int)bt3d_le32(&bytes[18]);
    height = (int)bt3d_le32(&bytes[22]);
    bits_per_pixel = bt3d_le16(&bytes[28]);
    compression = (int)bt3d_le32(&bytes[30]);
    if (dib_size < 40 || width <= 0 || height == 0) return image;
    if (!((bits_per_pixel == 4 && (compression == 0 || compression == 2))
        || (bits_per_pixel == 8 && (compression == 0 || compression == 1)))) return image;

    top_down = height < 0;
    if (top_down) height = -height;
    palette_entries = 1 << bits_per_pixel;
    palette_offset = 14 + dib_size;
    row_bytes = ((width * bits_per_pixel + 31) / 32) * 4;
    if (palette_offset + palette_entries * 4 > (int)size) return image;
    if (compression == 0 && (pixel_offset < 0 || pixel_offset + row_bytes * height > (int)size)) return image;

    for (i = 0; i < palette_entries; ++i) {
        int base = palette_offset + i * 4;
        palette[i] = apply_bitmap_color_key((Color){ bytes[base + 2], bytes[base + 1], bytes[base], 255 });
    }

    image = new_rgba_image(width, height);
    memset(image.data, 0, sizeof(Color) * (size_t)(width * height));
    if (compression == 0) {
        Color *pixels = (Color *)image.data;
        int row;
        for (row = 0; row < height; ++row) {
            const unsigned char *src = &bytes[pixel_offset + (top_down ? row : height - 1 - row) * row_bytes];
            int x;
            for (x = 0; x < width; ++x) {
                unsigned char index = bits_per_pixel == 8 ? src[x] : (unsigned char)((x % 2 == 0) ? (src[x / 2] >> 4) : (src[x / 2] & 0x0f));
                pixels[row * width + x] = palette[index];
            }
        }
        return image;
    }

    {
        int x = 0;
        int y = 0;
        byte_index = pixel_offset;
        while (byte_index + 1 < (int)size && y < height) {
            int count = bytes[byte_index++];
            int value = bytes[byte_index++];
            if (count > 0) {
                for (i = 0; i < count; ++i) {
                    unsigned char index = compression == 1 ? (unsigned char)value
                        : (unsigned char)(((i & 1) == 0) ? (value >> 4) : (value & 0x0f));
                    if (!bitmap_put_next_pixel(&image, top_down, &x, &y, palette[index])) break;
                }
            } else if (value == 0) {
                x = 0;
                y += 1;
            } else if (value == 1) {
                break;
            } else if (value == 2) {
                if (byte_index + 1 >= (int)size) break;
                x += bytes[byte_index++];
                y += bytes[byte_index++];
            } else {
                /* Absolute run of `value` pixels, padded to a 16-bit boundary. */
                int literal_bytes = compression == 1 ? value : (value + 1) / 2;
                if (byte_index + literal_bytes > (int)size) break;
                for (i = 0; i < value; ++i) {
                    unsigned char index = compression == 1 ? bytes[byte_index + i]
                        : (unsigned char)(((i & 1) == 0) ? (bytes[byte_index + i / 2] >> 4) : (bytes[byte_index + i / 2] & 0x0f));
                    if (!bitmap_put_next_pixel(&image, top_down, &x, &y, palette[index])) break;
                }
                byte_index += literal_bytes + (literal_bytes & 1);
            }
        }
    }
    return image;
}

static int has_prefix(const char *name, const char *prefix) {
    return strncmp(name, prefix, strlen(prefix)) == 0;
}

/* The <n> of NAME_<n>, or -1 when out of the 1..255 id range. */
static int entry_id(const char *name) {
    const char *underscore = strrchr(name, '_');
    int id = underscore ? atoi(underscore + 1) : -1;
    return id > 0 && id < 256 ? id : -1;
}

static void load_palette(const DatPack *pack, Color *palette) {
    const DatPackEntry *entry = datpack_find(pack, "BT_PAL");
    const unsigned char *bytes = entry ? datpack_entry_data(pack, entry) : NULL;
    int i;

    for (i = 0; i < 256; ++i) {
        palette[i] = (bytes && (size_t)(i * 3 + 2) < entry->length)
            ? (Color){ bytes[i * 3], bytes[i * 3 + 1], bytes[i * 3 + 2], 255 }
            : (Color){ 0, 0, 0, 255 };
    }
}

static void load_sound(AppState *app, int sound_id, const unsigned char *bytes, size_t size) {
    Wave wave;

    if (!app->audio.initialized || sound_id < 0 || sound_id >= ARRAY_COUNT(app->assets.sounds)) return;
    wave = LoadWaveFromMemory(".wav", bytes, (int)size);
    if (wave.frameCount == 0) return;
    app->assets.sounds[sound_id] = LoadSoundFromWave(wave);
    UnloadWave(wave);
}

void bt3d_load_assets(AppState *app) {
    const DatPack *pack = &app->content.pack;
    Color palette[256];
    size_t i;

    load_palette(pack, palette);
    memset(app->assets.wall_texture_entries, -1, sizeof(app->assets.wall_texture_entries));
    memset(app->assets.vec_texture_entries, -1, sizeof(app->assets.vec_texture_entries));
    app->assets.textures = (PackTexture *)calloc(pack->entry_count, sizeof(PackTexture));
    if (!app->assets.textures) return;

    for (i = 0; i < pack->entry_count; ++i) {
        const DatPackEntry *entry = &pack->entries[i];
        const unsigned char *bytes = datpack_entry_data(pack, entry);
        PackTexture *texture = &app->assets.textures[i];
        Image image = { 0 };
        int id = entry_id(entry->name);

        if (has_prefix(entry->name, "STN_")) {
            image = decode_stn(palette, bytes, entry->length);
            if (image.data && id > 0) app->assets.wall_texture_entries[id] = (short)i;
        } else if (has_prefix(entry->name, "VEC_")) {
            image = decode_patch(palette, bytes, entry->length, texture);
            if (image.data && id > 0) app->assets.vec_texture_entries[id] = (short)i;
        } else if (bt3d_is_sprite_entry_name(entry->name)) {
            image = decode_patch(palette, bytes, entry->length, texture);
        } else if (has_prefix(entry->name, "BM_") || has_prefix(entry->name, "M_")) {
            image = decode_bitmap(bytes, entry->length);
        } else if (has_prefix(entry->name, "SND_")) {
            load_sound(app, entry_id(entry->name), bytes, entry->length);
        }

        if (image.data) {
            texture->texture = LoadTextureFromImage(image);
            SetTextureFilter(texture->texture, TEXTURE_FILTER_POINT);
            UnloadImage(image);
        }
    }
}

void bt3d_unload_assets(AppState *app) {
    size_t i;

    if (app->assets.textures) {
        for (i = 0; i < app->content.pack.entry_count; ++i) {
            if (app->assets.textures[i].texture.id) UnloadTexture(app->assets.textures[i].texture);
        }
        free(app->assets.textures);
        app->assets.textures = NULL;
    }
    for (i = 0; i < (size_t)ARRAY_COUNT(app->assets.sounds); ++i) {
        if (app->assets.sounds[i].frameCount) UnloadSound(app->assets.sounds[i]);
    }
    memset(app->assets.sounds, 0, sizeof(app->assets.sounds));
}

static const PackTexture *loaded_texture(const AppState *app, long index) {
    const PackTexture *texture;
    if (index < 0 || !app->assets.textures) return NULL;
    texture = &app->assets.textures[index];
    return texture->texture.id ? texture : NULL;
}

const PackTexture *bt3d_texture(const AppState *app, const char *entry_name) {
    const DatPackEntry *entry = datpack_find(&app->content.pack, entry_name);
    return entry ? loaded_texture(app, entry - app->content.pack.entries) : NULL;
}

const PackTexture *bt3d_wall_texture(const AppState *app, int texture_id) {
    if (texture_id <= 0 || texture_id >= 256) return NULL;
    return loaded_texture(app, app->assets.wall_texture_entries[texture_id]);
}

const PackTexture *bt3d_vec_texture(const AppState *app, int vec_id) {
    if (vec_id <= 0 || vec_id >= 256) return NULL;
    return loaded_texture(app, app->assets.vec_texture_entries[vec_id]);
}
