#ifndef AB_TEST_ASSET_TEXTURES_COMMON_H
#define AB_TEST_ASSET_TEXTURES_COMMON_H

#include "ab/asset_textures.h"
#include "ab/common_renderer.h"
#include "ab/sprite_resource.h"

#include <stdio.h>
#include <string.h>

static AbDrawCommand asset_texture_gpu_quad(uint32_t texture) {
    return (AbDrawCommand){.texture = texture,
                           .quad = {{{-1, 1, 0.5f, 0.5f},
                                     {1, 1, 0.5f, 0.5f},
                                     {-1, -1, 0.5f, 0.5f},
                                     {1, -1, 0.5f, 0.5f}}},
                           .color = {1, 1, 1, 1},
                           .opacity = 1,
                           .coordinates = AB_DRAW_CLIP};
}

static bool asset_texture_gpu_menu(CcPlatform *platform, AbCommonRenderer *renderer,
                                   AbAssetTextures *textures,
                                   const AbResourceOwners *owners, bool capture,
                                   AbError *error) {
    static const uint8_t name[] = "MENU_GRADIENTS";
    const AbResourceSource *source = ab_resource_owners_find(
        owners, AB_SPRITE_REGISTRY_SPRITE, (AbSpan){name, sizeof(name) - 1});
    const AbSpriteAtlas *atlas =
        ab_sprite_resource_atlas(ab_sprite_resource_from_source(source));
    AbSpriteAtlasImage image;
    uint32_t texture;
    if (!ab_sprite_atlas_image(atlas, 0, &image) ||
        !ab_asset_textures_find(textures, image.image, &texture) ||
        !(image.image->width & 1) || !(image.image->height & 1)) {
        return ab_fail(error, "Cannot bind odd-dimension menu texture for GPU check");
    }
    size_t center = ((size_t)(image.image->height / 2) * image.image->width +
                     image.image->width / 2) *
                    4;
    uint8_t expected[4];
    memcpy(expected, image.image->rgba.data + center, sizeof(expected));
    AbDrawCommand command = asset_texture_gpu_quad(texture);
    AbDrawList list = {&command, 1, 1};
    const float clear[4] = {0, 0, 0, 1};
    if (!capture) {
        return ab_common_render(renderer, &list, clear, error);
    }
    CcFramebuffer frame;
    if (!cc_platform_capture_begin(platform, &frame)) {
        return ab_fail(error, "Cannot begin Common GPU texture capture");
    }
    bool okay = ab_common_render(renderer, &list, clear, error) &&
                cc_platform_capture_frame(platform, &frame);
    if (okay &&
        (!frame.rgba || frame.width <= 0 || frame.height <= 0 || frame.width > 4096 ||
         frame.height > 4096 || frame.stride < (size_t)frame.width * 4)) {
        okay = ab_fail(error, "Invalid Common GPU texture capture dimensions");
    }
    for (int row = 0; okay && row < frame.height; ++row) {
        for (int column = 0; column < frame.width; ++column) {
            const uint8_t *actual =
                frame.rgba + (size_t)row * frame.stride + (size_t)column * 4;
            if (memcmp(actual, expected, 4)) {
                okay = ab_fail(error,
                               "Common GPU menu pixel differs at %d,%d: "
                               "[%u,%u,%u,%u]/[%u,%u,%u,%u]",
                               column, row, actual[0], actual[1], actual[2], actual[3],
                               expected[0], expected[1], expected[2], expected[3]);
                break;
            }
        }
    }
    cc_platform_capture_end(platform);
    if (okay) {
        printf("Matched Common GPU menu sample across %d pixels.\n",
               frame.width * frame.height);
    }
    return okay;
}

static bool asset_texture_gpu_removed(AbCommonRenderer *renderer, uint32_t texture,
                                      AbError *error) {
    AbDrawCommand command = asset_texture_gpu_quad(texture);
    AbDrawList list = {&command, 1, 1};
    const float clear[4] = {0, 0, 0, 1};
    if (ab_common_render(renderer, &list, clear, NULL)) {
        return ab_fail(error, "Common GPU accepted a removed asset texture");
    }
    return ab_common_unload(renderer, texture, error);
}

#endif
