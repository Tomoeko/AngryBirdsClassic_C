#include "classic_preview_assets.h"
#include "ab/hash.h"

#include <stdio.h>
#include <stdlib.h>

static bool write_word(FILE *file, uint32_t value) {
    const uint8_t bytes[] = {(uint8_t)value, (uint8_t)(value >> 8),
                             (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static bool write_float(FILE *file, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return write_word(file, bits);
}

static bool write_texture(FILE *file, uint32_t texture, const AbImage *image) {
    return image && image->rgba.size <= UINT32_MAX && write_word(file, texture) &&
           write_word(file, image->width) && write_word(file, image->height) &&
           write_word(file, (uint32_t)image->rgba.size) &&
           fwrite(image->rgba.data, 1, image->rgba.size, file) == image->rgba.size;
}

static bool write_commands(FILE *file, const AbDrawList *list) {
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (!write_word(file, command->blend) || !write_word(file, command->texture) ||
            !write_word(file, command->coordinates)) {
            return false;
        }
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            const AbDrawVertex *item = command->quad.vertices + vertex;
            if (!write_float(file, item->x) || !write_float(file, item->y) ||
                !write_float(file, item->u) || !write_float(file, item->v)) {
                return false;
            }
        }
        for (size_t channel = 0; channel < 4; ++channel) {
            if (!write_float(file, command->color[channel])) {
                return false;
            }
        }
        if (!write_float(file, command->opacity)) {
            return false;
        }
    }
    return true;
}

static bool write_scene(const char *path, const PreviewLevel *level,
                        const AbDrawList *list, AbError *error) {
    FILE *file = fopen(path, "wbx");
    if (!file) {
        return ab_fail(error, "Cannot create new private GPU input");
    }
    static const char magic[] = "AB_THEME_GPU_V1\n";
    size_t texture_count = level_preview_texture_count(level);
    float clear[4];
    theme_preview_clear(&level->theme, clear);
    bool okay = texture_count <= UINT32_MAX && list->count <= UINT32_MAX &&
                fwrite(magic, 1, sizeof(magic) - 1, file) == sizeof(magic) - 1 &&
                write_word(file, 480) && write_word(file, 320) &&
                write_word(file, (uint32_t)texture_count) &&
                write_word(file, (uint32_t)list->count);
    for (size_t channel = 0; okay && channel < 4; ++channel) {
        okay = write_float(file, clear[channel]);
    }
    for (size_t index = 0; okay && index < texture_count; ++index) {
        const AbImage *image;
        uint32_t texture;
        okay = level_preview_texture(level, index, &texture, &image) &&
               write_texture(file, texture, image);
    }
    okay = okay && write_commands(file, list);
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        remove(path);
    }
    return okay || ab_fail(error, "Cannot write complete private GPU input");
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fputs("Usage: ab_theme_gpu_data APK NEW_PRIVATE_OUTPUT [HIGHSCORE]\n", stderr);
        return 2;
    }
    AbError error = {0};
    AbBuffer bytes = {0};
    AbZip archive = {0};
    PreviewLevel level = {0};
    AbDrawList list = {0};
    uint8_t digest[32];
    char hash[65];
    bool okay = ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_sha256((AbSpan){bytes.data, bytes.size}, digest, &error);
    if (okay) {
        ab_digest_hex(digest, hash);
        okay =
            strcmp(
                hash,
                "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") ==
            0;
        if (!okay) {
            ab_fail(&error, "GPU input requires the pinned APK");
        }
    }
    okay = okay && ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
           level_preview_open(&archive, &level, &error) &&
           ab_draw_list_init(PREVIEW_THEME_DRAWS + 32, 128 * 1024, &list, &error);
    if (okay && argc == 4) {
        char *end;
        unsigned long value = strtoul(argv[3], &end, 10);
        okay =
            argv[3][0] >= '0' && argv[3][0] <= '9' && *end == 0 && value < 2147483648ul;
        if (okay) {
            level.hud.input.highscore = (float)value;
            okay = level.hud.input.highscore < 2147483648.0f;
            level.hud.input.highscore_present = true;
        }
        if (!okay) {
            ab_fail(&error, "Invalid highscore input");
        }
    }
    for (size_t frame = 0; okay && frame < 128; ++frame) {
        okay = level_preview_draw(&level, 0x1.111112p-5f, &list, NULL, &error);
    }
    okay = okay && write_scene(argv[2], &level, &list, &error);
    if (okay) {
        printf("Prepared %zu commands and %zu decoded textures for private GPU "
               "comparison.\n",
               list.count, level_preview_texture_count(&level));
    } else {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_draw_list_free(&list);
    level_preview_free(&level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
