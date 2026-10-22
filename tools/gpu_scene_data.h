#ifndef AB_TOOLS_GPU_SCENE_DATA_H
#define AB_TOOLS_GPU_SCENE_DATA_H

#include "ab/draw.h"
#include "ab/image.h"

#include <math.h>
#include <string.h>

/* Private diagnostic format, not an original APK resource. Images borrow the
 * input bytes. Keep the parsed owner at the same address because its list
 * references its inline commands. No allocation or original code execution. */
typedef struct {
    uint32_t width, height, texture_count;
    float clear[4];
    AbImage images[16];
    AbDrawCommand commands[128];
    AbDrawList list;
} AbGpuSceneData;

typedef struct {
    AbSpan bytes;
    size_t offset;
} AbGpuSceneCursor;

static bool gpu_scene_word(AbGpuSceneCursor *cursor, uint32_t *output) {
    if (!ab_range(cursor->bytes.size, cursor->offset, 4)) {
        return false;
    }
    *output = ab_u32le(cursor->bytes.data + cursor->offset);
    cursor->offset += 4;
    return true;
}

static bool gpu_scene_float(AbGpuSceneCursor *cursor, float *output) {
    uint32_t word;
    if (!gpu_scene_word(cursor, &word)) {
        return false;
    }
    memcpy(output, &word, sizeof(word));
    return true;
}

static bool ab_gpu_scene_data_read(AbSpan bytes, AbGpuSceneData *scene,
                                   AbError *error) {
    static const char magic[] = "AB_THEME_GPU_V1\n";
    AbGpuSceneCursor cursor = {bytes, sizeof(magic) - 1};
    uint32_t count;
    if (bytes.size < sizeof(magic) - 1 ||
        memcmp(bytes.data, magic, sizeof(magic) - 1) ||
        !gpu_scene_word(&cursor, &scene->width) ||
        !gpu_scene_word(&cursor, &scene->height) || scene->width != 480 ||
        scene->height != 320 || !gpu_scene_word(&cursor, &scene->texture_count) ||
        !scene->texture_count || scene->texture_count > 16 ||
        !gpu_scene_word(&cursor, &count) || !count || count > 128) {
        return ab_fail(error, "Invalid bounded GPU scene header");
    }
    scene->list = (AbDrawList){scene->commands, count, 128};
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!gpu_scene_float(&cursor, scene->clear + channel) ||
            !isfinite(scene->clear[channel]) || scene->clear[channel] < 0 ||
            scene->clear[channel] > 1) {
            return ab_fail(error, "Invalid GPU clear color");
        }
    }
    for (uint32_t index = 0; index < scene->texture_count; ++index) {
        uint32_t id, size;
        AbImage *image = scene->images + index;
        if (!gpu_scene_word(&cursor, &id) || id != index + 1 ||
            !gpu_scene_word(&cursor, &image->width) ||
            !gpu_scene_word(&cursor, &image->height) ||
            !gpu_scene_word(&cursor, &size) || !image->width || !image->height ||
            image->width > 4096 || image->height > 4096 ||
            size != image->width * image->height * 4 ||
            !ab_range(bytes.size, cursor.offset, size)) {
            return ab_fail(error, "Invalid bounded GPU texture");
        }
        /* Images borrow the immutable private input until GPU upload completes. */
        image->rgba = (AbBuffer){(uint8_t *)bytes.data + cursor.offset, size};
        cursor.offset += size;
    }
    for (uint32_t index = 0; index < count; ++index) {
        AbDrawCommand *command = scene->commands + index;
        uint32_t blend, coordinates;
        if (!gpu_scene_word(&cursor, &blend) || blend > AB_BLEND_MULTIPLY ||
            !gpu_scene_word(&cursor, &command->texture) ||
            !gpu_scene_word(&cursor, &coordinates) || coordinates > AB_DRAW_CLIP) {
            return ab_fail(error, "Invalid bounded GPU command");
        }
        command->blend = (AbBlend)blend;
        command->coordinates = (AbDrawCoordinates)coordinates;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            AbDrawVertex *item = command->quad.vertices + vertex;
            if (!gpu_scene_float(&cursor, &item->x) ||
                !gpu_scene_float(&cursor, &item->y) ||
                !gpu_scene_float(&cursor, &item->u) ||
                !gpu_scene_float(&cursor, &item->v)) {
                return ab_fail(error, "Truncated GPU vertices");
            }
        }
        for (size_t channel = 0; channel < 4; ++channel) {
            if (!gpu_scene_float(&cursor, command->color + channel) ||
                !isfinite(command->color[channel])) {
                return ab_fail(error, "Invalid GPU color");
            }
        }
        if (!gpu_scene_float(&cursor, &command->opacity)) {
            return ab_fail(error, "Truncated GPU opacity");
        }
        if (!command->texture || command->texture > scene->texture_count ||
            !ab_draw_command_check(command, error)) {
            return ab_fail(error, "Invalid GPU draw command");
        }
    }
    return cursor.offset == bytes.size || ab_fail(error, "Unexpected GPU input tail");
}

#endif
