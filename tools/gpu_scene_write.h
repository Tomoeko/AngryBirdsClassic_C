#ifndef AB_TOOLS_GPU_SCENE_WRITE_H
#define AB_TOOLS_GPU_SCENE_WRITE_H

#include "ab/draw.h"
#include "ab/image.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static inline bool gpu_write_word(FILE *file, uint32_t value) {
    const uint8_t bytes[] = {(uint8_t)value, (uint8_t)(value >> 8),
                             (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static inline bool gpu_write_float(FILE *file, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return gpu_write_word(file, bits);
}

/* Private diagnostic format shared with gpu_scene_data.h. Borrow images and
 * commands; fixed logical viewport, sequential texture IDs, <32 MiB total input.
 * Create only a new file and remove partial output on write failure. */
static inline bool gpu_scene_write(const char *path, const AbImage *const *images,
                                   size_t image_count, const AbDrawList *list,
                                   const float clear[4], AbError *error) {
    if (!path || !images || !image_count || image_count > 16 || !list ||
        !list->commands || !list->count || list->count > 128 ||
        list->count > list->capacity || !clear)
        return ab_fail(error, "Invalid private GPU scene source");
    size_t total_bytes = 0;
    for (size_t index = 0; index < image_count; ++index) {
        const AbImage *image = images[index];
        if (!image || !image->width || !image->height || image->width > 4096 ||
            image->height > 4096 || !image->rgba.data ||
            image->rgba.size != (size_t)image->width * image->height * 4 ||
            image->rgba.size > 32 * 1024 * 1024 - 16384 - total_bytes)
            return ab_fail(error, "Invalid private GPU scene image");
        total_bytes += image->rgba.size;
    }
    for (size_t channel = 0; channel < 4; ++channel)
        if (!isfinite(clear[channel]) || clear[channel] < 0 || clear[channel] > 1)
            return ab_fail(error, "Invalid private GPU scene clear color");
    for (size_t index = 0; index < list->count; ++index)
        if (list->commands[index].texture > image_count ||
            !ab_draw_command_check(list->commands + index, error))
            return ab_fail(error, "Invalid private GPU scene command");
    FILE *file = fopen(path, "wbx");
    if (!file)
        return ab_fail(error, "Cannot create new private GPU scene");
    static const char magic[] = "AB_THEME_GPU_V1\n";
    bool okay = fwrite(magic, 1, sizeof(magic) - 1, file) == sizeof(magic) - 1 &&
                gpu_write_word(file, 480) && gpu_write_word(file, 320) &&
                gpu_write_word(file, (uint32_t)image_count) &&
                gpu_write_word(file, (uint32_t)list->count);
    for (size_t channel = 0; okay && channel < 4; ++channel)
        okay = gpu_write_float(file, clear[channel]);
    for (size_t index = 0; okay && index < image_count; ++index) {
        const AbImage *image = images[index];
        okay = gpu_write_word(file, (uint32_t)index + 1) &&
               gpu_write_word(file, image->width) &&
               gpu_write_word(file, image->height) &&
               gpu_write_word(file, (uint32_t)image->rgba.size) &&
               fwrite(image->rgba.data, 1, image->rgba.size, file) == image->rgba.size;
    }
    for (size_t index = 0; okay && index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        okay = gpu_write_word(file, command->blend) &&
               gpu_write_word(file, command->texture) &&
               gpu_write_word(file, command->coordinates);
        for (size_t vertex = 0; okay && vertex < 4; ++vertex) {
            const AbDrawVertex *item = command->quad.vertices + vertex;
            okay = gpu_write_float(file, item->x) && gpu_write_float(file, item->y) &&
                   gpu_write_float(file, item->u) && gpu_write_float(file, item->v);
        }
        for (size_t channel = 0; okay && channel < 4; ++channel)
            okay = gpu_write_float(file, command->color[channel]);
        okay = okay && gpu_write_float(file, command->opacity);
    }
    if (fclose(file) != 0)
        okay = false;
    if (!okay)
        remove(path);
    return okay || ab_fail(error, "Cannot write complete private GPU scene");
}

#endif
