#include "ab/common_renderer.h"

#include "console_common/render/viewport.h"

#include <stdio.h>
#include <string.h>

/* Explicit native-window check. No audio, synthetic input or
 * default CTest registration. Capture the actual fitted/backing-pixel viewport. */
int main(int argc, char **argv) {
    bool native = argc == 2 && !strcmp(argv[1], "--native");
    if (argc > 2 || (argc == 2 && !native))
        return 2;
    AbError error = {0};
    CcPlatform *platform =
        cc_platform_create("Angry Birds Classic C clip check", CC_FRAME_WIDTH + 120,
                           CC_FRAME_HEIGHT + 140);
    AbCommonRenderer *renderer = NULL;
    bool okay = false;
    if (!platform || !ab_common_create(platform, 4, 1, 32768, &renderer, &error))
        goto release;
    uint8_t white[] = {255, 255, 255, 255};
    AbImage image = {1, 1, {white, sizeof(white)}};
    if (!ab_common_upload(renderer, 1, &image, false, false, &error))
        goto release;
    int width = CC_FRAME_WIDTH, height = CC_FRAME_HEIGHT;
    if (native && !cc_platform_drawable_size(platform, &width, &height)) {
        ab_fail(&error, "Cannot query native clip drawable");
        goto release;
    }
    cc_platform_set_native_pointer(platform, native);
    float w = (float)width, h = (float)height;
    AbDrawQuad quad = {{{0, 0, 0, 0}, {w, 0, 1, 0}, {0, h, 0, 1}, {w, h, 1, 1}}};
    AbDrawCommand commands[] = {
        {.blend = AB_BLEND_OPAQUE,
         .texture = 1,
         .quad = quad,
         .color = {1, 0, 0, 1},
         .opacity = 1},
        {.blend = AB_BLEND_OPAQUE,
         .texture = 1,
         .quad = quad,
         .color = {0, 1, 0, 1},
         .opacity = 1,
         .scissor = {width / 4, height / 4, width * 3 / 4, height * 3 / 4, true}},
        {.blend = AB_BLEND_OPAQUE,
         .texture = 1,
         .quad = quad,
         .color = {0, 0, 1, 1},
         .opacity = 1,
         .scissor = {-40, height / 2, width / 2, height + 80, true}},
        {.blend = AB_BLEND_OPAQUE,
         .texture = 1,
         .quad = quad,
         .color = {1, 1, 1, 1},
         .opacity = 1,
         .scissor = {width + 1, 0, width + 10, 10, true}}};
    AbDrawList list = {commands, 4, 4};
    CcFramebuffer frame;
    const float clear[] = {0, 0, 0, 1};
    /* Capture waits for every submission and masks ordinary slot reuse. Exercise
     * the asynchronous presentation path before enabling any readback. */
    for (unsigned index = 0; index < 256; ++index) {
        bool rendered = native ? ab_common_render_native(renderer, &list, clear, width,
                                                         height, &error)
                               : ab_common_render(renderer, &list, clear, &error);
        if (!rendered)
            goto release;
    }
    if (!cc_platform_capture_begin(platform, &frame)) {
        ab_fail(&error, "Cannot begin Common clip capture");
        goto release;
    }
    for (unsigned index = 0; index < 2; ++index) {
        CcEvent event;
        while (cc_platform_poll(platform, &event)) {
            if (event.type == CC_EVENT_QUIT) {
                ab_fail(&error, "Common clip window closed before capture");
                goto release;
            }
        }
        bool rendered = native ? ab_common_render_native(renderer, &list, clear, width,
                                                         height, &error)
                               : ab_common_render(renderer, &list, clear, &error);
        if (!rendered)
            goto release;
    }
    if (!cc_platform_capture_frame(platform, &frame) || !frame.rgba ||
        frame.width < 1 || frame.height < 1 || frame.width > 8192 ||
        frame.height > 8192 || frame.stride != (size_t)frame.width * 4) {
        ab_fail(&error, "Invalid Common clip capture");
        goto release;
    }
    CcViewport viewport = native ? (CcViewport){0, 0, frame.width, frame.height}
                                 : cc_viewport_fit(frame.width, frame.height);
    size_t checked = 0;
    for (int y = 0; y < frame.height; ++y)
        for (int x = 0; x < frame.width; ++x) {
            uint8_t expected[] = {0, 0, 0, 255};
            double lx = ((double)x + 0.5 - viewport.x) / viewport.width;
            double ly = ((double)y + 0.5 - viewport.y) / viewport.height;
            if (lx >= 0 && lx < 1 && ly >= 0 && ly < 1) {
                expected[0] = 255;
                if (lx >= 0.25 && lx < 0.75 && ly >= 0.25 && ly < 0.75) {
                    expected[0] = 0;
                    expected[1] = 255;
                }
                if (lx < 0.5 && ly >= 0.5) {
                    expected[0] = expected[1] = 0;
                    expected[2] = 255;
                }
            }
            const uint8_t *pixel =
                frame.rgba + (size_t)y * frame.stride + (size_t)x * 4;
            if (memcmp(pixel, expected, sizeof(expected))) {
                ab_fail(&error, "Common clip pixel differs at %d,%d: %u,%u,%u,%u", x, y,
                        pixel[0], pixel[1], pixel[2], pixel[3]);
                goto release;
            }
            ++checked;
        }
    if (!ab_common_wait(renderer, &error))
        goto release;
    printf("Common clip: %zu pixels match at %dx%d, fitted viewport %d,%d %dx%d.\n",
           checked, frame.width, frame.height, viewport.x, viewport.y, viewport.width,
           viewport.height);
    okay = true;
release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    if (platform)
        cc_platform_capture_end(platform);
    ab_common_free(renderer);
    cc_platform_destroy(platform);
    return okay ? 0 : 1;
}
