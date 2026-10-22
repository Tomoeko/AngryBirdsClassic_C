#ifndef AB_TEST_CLASSIC_SCENE_COMMON_H
#define AB_TEST_CLASSIC_SCENE_COMMON_H

#include "ab/common_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool resource_scene_gpu_open(const AbSpriteAtlas *atlas, CcPlatform **platform,
                                    AbCommonRenderer **renderer, AbError *error) {
    *platform = NULL;
    *renderer = NULL;
    if (ab_sprite_atlas_count(atlas) > 25) {
        return ab_fail(error, "Too many reference scene textures");
    }
    *platform = cc_platform_create("Angry Birds resource scene validation", 240, 160);
    if (!*platform ||
        !ab_common_create(*platform, 23, 64, 1024 * 1024, renderer, error)) {
        return ab_fail(error, "Cannot create Common resource scene renderer");
    }
    for (size_t index = 0; index < ab_sprite_atlas_count(atlas); ++index) {
        AbSpriteAtlasImage image;
        if (!ab_sprite_atlas_image(atlas, index, &image) ||
            !ab_common_upload(*renderer, 39 + (uint32_t)index, image.image, false,
                              false, error)) {
            return false;
        }
    }
    return true;
}

static bool resource_scene_gpu_capture(CcPlatform *platform, AbCommonRenderer *renderer,
                                       const AbDrawList *list, AbBuffer *output,
                                       AbVec2 *size, AbError *error) {
    *output = (AbBuffer){0};
    CcFramebuffer frame;
    if (!cc_platform_capture_begin(platform, &frame)) {
        return ab_fail(error, "Cannot begin Common scene capture");
    }
    const float clear[4] = {0, 0, 0, 1};
    bool okay = ab_common_render(renderer, list, clear, error) &&
                cc_platform_capture_frame(platform, &frame);
    if (okay &&
        (!frame.rgba || frame.width <= 0 || frame.height <= 0 || frame.width > 4096 ||
         frame.height > 4096 || frame.stride < (size_t)frame.width * 4 ||
         (size_t)frame.width * (size_t)frame.height > 4 * 1024 * 1024)) {
        okay = ab_fail(error, "Invalid Common scene capture dimensions");
    }
    if (okay) {
        size_t row_bytes = (size_t)frame.width * 4;
        output->size = row_bytes * (size_t)frame.height;
        output->data = malloc(output->size);
        if (!output->data) {
            output->size = 0;
            okay = ab_fail(error, "Cannot retain Common scene capture");
        } else {
            for (int row = 0; row < frame.height; ++row) {
                memcpy(output->data + (size_t)row * row_bytes,
                       frame.rgba + (size_t)row * frame.stride, row_bytes);
            }
            *size = (AbVec2){(float)frame.width, (float)frame.height};
        }
    }
    cc_platform_capture_end(platform);
    return okay;
}

/* Both paths consume the same recovered geometry and explicit clamp sampler.
 * This checks real GPU resource selection/upload across unrelated logical IDs;
 * the fixed-atlas path is a composition control, not an original-frame oracle. */
static bool resource_scene_gpu_compare(CcPlatform *platform, AbCommonRenderer *renderer,
                                       const AbDrawList *actual,
                                       const AbDrawList *expected, AbError *error) {
    if (!actual->count || actual->count > 22 || actual->count != expected->count) {
        return ab_fail(error, "Invalid ordinary GPU scene command counts");
    }
    AbDrawCommand remapped[22];
    memcpy(remapped, expected->commands, expected->count * sizeof(*remapped));
    for (size_t index = 0; index < expected->count; ++index) {
        if (!remapped[index].texture || remapped[index].texture > 32) {
            return ab_fail(error, "Invalid fixed-atlas GPU scene texture");
        }
        remapped[index].texture += 32;
    }
    AbDrawList reference = {remapped, expected->count, 22};
    AbBuffer actual_pixels = {0}, expected_pixels = {0};
    AbVec2 actual_size = {0}, expected_size = {0};
    bool okay = resource_scene_gpu_capture(platform, renderer, actual, &actual_pixels,
                                           &actual_size, error) &&
                resource_scene_gpu_capture(platform, renderer, &reference,
                                           &expected_pixels, &expected_size, error);
    if (okay &&
        (actual_size.x != expected_size.x || actual_size.y != expected_size.y ||
         actual_pixels.size != expected_pixels.size ||
         memcmp(actual_pixels.data, expected_pixels.data, actual_pixels.size) != 0)) {
        okay = ab_fail(error, "Resource and fixed-atlas GPU scene pixels differ");
    }
    size_t nonblack = 0;
    for (size_t offset = 0; okay && offset < actual_pixels.size; offset += 4) {
        nonblack += actual_pixels.data[offset] || actual_pixels.data[offset + 1] ||
                    actual_pixels.data[offset + 2];
    }
    if (okay && !nonblack) {
        okay = ab_fail(error, "Common GPU scene capture has no visible geometry");
    }
    if (okay) {
        printf("Common GPU resource scene matches fixed-atlas control across %zu "
               "pixels (%zu draws, %zu nonblack).\n",
               actual_pixels.size / 4, actual->count, nonblack);
    }
    ab_buffer_free(&expected_pixels);
    ab_buffer_free(&actual_pixels);
    return okay;
}

#endif
