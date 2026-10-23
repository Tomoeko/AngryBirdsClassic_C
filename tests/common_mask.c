#include "ab/common_renderer.h"

#include <stdio.h>
#include <string.h>

/* Explicit real-window material check; keep it out of default headless CTest. */
int main(void) {
    AbError error = {0};
    CcPlatform *platform =
        cc_platform_create("Angry Birds terrain material check", 320, 240);
    AbCommonRenderer *renderer = NULL;
    bool okay = false, capturing = false;
    CcFramebuffer frame = {0};
    int width, height;
    uint8_t red[] = {255, 0, 0, 255}, mask_pixels[] = {0, 255, 0, 128};
    AbImage terrain = {1, 1, {red, sizeof(red)}};
    AbImage mask = {1, 1, {mask_pixels, sizeof(mask_pixels)}};
    if (!platform || !cc_platform_drawable_size(platform, &width, &height) ||
        !ab_common_create(platform, 2, 2, 32768, &renderer, &error) ||
        !ab_common_upload(renderer, 1, &terrain, true, true, &error) ||
        !ab_common_upload(renderer, 2, &mask, false, false, &error))
        goto release;
    AbDrawQuad quad = {
        {{-1, 1, -2, -2}, {1, 1, 2, -2}, {-1, -1, -2, 2}, {1, -1, 2, 2}}};
    AbDrawCommand commands[] = {
        {.blend = AB_BLEND_STRAIGHT,
         .texture = 1,
         .quad = quad,
         .color = {1, 1, 1, 1},
         .opacity = 1,
         .coordinates = AB_DRAW_CLIP,
         .mask = {.texture = 2,
                  .uv = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}}},
        {.blend = AB_BLEND_OPAQUE,
         .texture = 2,
         .quad = quad,
         .color = {1, 1, 1, 1},
         .opacity = 1,
         .coordinates = AB_DRAW_CLIP,
         .scissor = {width * 3 / 4, 0, width, height, true}}};
    AbDrawList list = {commands, 2, 2};
    float clear[] = {0, 0, 1, 1};
    if (!cc_platform_capture_begin(platform, &frame))
        goto release;
    capturing = true;
    for (size_t index = 0; index < 3; ++index)
        if (!ab_common_render_native(renderer, &list, clear, width, height, &error))
            goto release;
    if (!cc_platform_capture_frame(platform, &frame) || frame.width != width ||
        frame.height != height || !frame.rgba || frame.stride != (size_t)width * 4)
        goto release;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const uint8_t expected_masked[] = {128, 0, 127, 191};
            const uint8_t expected_plain[] = {0, 255, 0, 128};
            const uint8_t *expected =
                x >= width * 3 / 4 ? expected_plain : expected_masked;
            const uint8_t *pixel =
                frame.rgba + (size_t)y * frame.stride + (size_t)x * 4;
            if (memcmp(pixel, expected, 4)) {
                ab_fail(&error, "Common terrain material pixel %d,%d: %u,%u,%u,%u", x,
                        y, pixel[0], pixel[1], pixel[2], pixel[3]);
                goto release;
            }
        }
    okay = ab_common_wait(renderer, &error);
    if (okay)
        printf("Common mask: %dx%d pixels match alpha-only masking, straight "
               "blending and subsequent ordinary drawing.\n",
               width, height);
release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    if (capturing)
        cc_platform_capture_end(platform);
    ab_common_free(renderer);
    cc_platform_destroy(platform);
    return okay ? 0 : 1;
}
