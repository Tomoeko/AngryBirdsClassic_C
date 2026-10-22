#include "ab/metal.h"

#include "../src/platform/apple/objects.h"

#include <stdio.h>
#include <string.h>

extern id MTLCreateSystemDefaultDevice(void);
extern id MTLCopyAllDevices(void);

typedef struct {
    double red;
    double green;
    double blue;
    double alpha;
} TestClearColor;

static bool render_pixels(id device, AbMetal *renderer, AbMetalFormat format,
                          const AbDrawList *list, TestClearColor clear,
                          uint8_t output[16], AbError *error) {
    (void)device;
    (void)format;
    const float channels[] = {(float)clear.red, (float)clear.green, (float)clear.blue,
                              (float)clear.alpha};
    AbImage image;
    if (!ab_metal_capture(renderer, list, 2, 2, channels, 16, &image, error)) {
        return false;
    }
    bool okay = image.width == 2 && image.height == 2 && image.rgba.size == 16;
    if (okay) {
        memcpy(output, image.rgba.data, 16);
    }
    ab_image_free(&image);
    return okay || ab_fail(error, "Metal capture dimensions differ");
}

static bool all_pixels(AbSpan pixels, const uint8_t expected[4], AbError *error) {
    for (size_t index = 0; index < pixels.size; index += 4) {
        if (memcmp(pixels.data + index, expected, 4) != 0) {
            return ab_fail(error,
                           "Metal pixel %zu: got [%u,%u,%u,%u], expected "
                           "[%u,%u,%u,%u]",
                           index / 4, pixels.data[index], pixels.data[index + 1],
                           pixels.data[index + 2], pixels.data[index + 3], expected[0],
                           expected[1], expected[2], expected[3]);
        }
    }
    return true;
}

static bool gpu_sprite_checks(id device, AbMetalFormat format, AbError *error) {
    AbMetal *renderer = NULL;
    AbDrawList list = {0};
    bool okay = false;
    if (!ab_metal_create(device, format, 2, 8192, &renderer, error) ||
        !ab_draw_list_init(2, 4096, &list, error)) {
        goto release_renderer;
    }
    uint8_t pixels[16] = {255, 0, 0,   255, 0,   255, 0,   255,
                          0,   0, 255, 255, 255, 255, 255, 255};
    AbImage image = {2, 2, {pixels, sizeof(pixels)}};
    uint8_t translucent[4] = {64, 32, 16, 128};
    AbImage alpha = {1, 1, {translucent, sizeof(translucent)}};
    if (!ab_metal_upload(renderer, 1, &image, false, false, error) ||
        !ab_metal_upload(renderer, 2, &alpha, false, false, error)) {
        goto release_renderer;
    }
    AbSpriteRegion region = {.width = 2, .height = 2};
    AbSpritePlacement placement = {0, 0, 1, 1, AB_ANCHOR_TOP, AB_ANCHOR_LEFT};
    AbDrawCommand command = {.texture = 1, .color = {1, 1, 1, 1}, .opacity = 1};
    if (!ab_sprite_quad(&region, 2, 2, placement, &command.quad, error) ||
        !ab_draw_list_push(&list, &command, error)) {
        goto release_renderer;
    }
    uint8_t rendered[16];
    TestClearColor clear = {0, 0, 1, 1};
    if (!render_pixels(device, renderer, format, &list, clear, rendered, error)) {
        goto release_renderer;
    }
    if (memcmp(pixels, rendered, sizeof(pixels)) != 0) {
        ab_fail(error, "Metal opaque texture roundtrip or orientation differs");
        goto release_renderer;
    }
    list.commands[0].scissor = (AbDrawClip){1, 0, 2, 1, true};
    const uint8_t cropped[] = {0, 0, 255, 255, 0, 255, 0,   255,
                               0, 0, 255, 255, 0, 0,   255, 255};
    if (!render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        memcmp(rendered, cropped, sizeof(cropped))) {
        ab_fail(error, "Metal screen scissor differs");
        goto release_renderer;
    }
    list.commands[0].scissor = (AbDrawClip){-4, -3, 0, 0, true};
    const uint8_t blue[] = {0, 0, 255, 255};
    if (!render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        !all_pixels((AbSpan){rendered, sizeof(rendered)}, blue, error))
        goto release_renderer;
    list.commands[0].scissor = (AbDrawClip){0};
    AbDrawCommand clip = command;
    clip.coordinates = AB_DRAW_CLIP;
    const float clip_positions[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};
    for (size_t index = 0; index < 4; ++index) {
        clip.quad.vertices[index].x = clip_positions[index][0];
        clip.quad.vertices[index].y = clip_positions[index][1];
    }
    ab_draw_list_clear(&list);
    if (!ab_draw_list_push(&list, &clip, error) ||
        !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        memcmp(pixels, rendered, sizeof(pixels)) != 0 ||
        !ab_draw_list_push(&list, &command, error) ||
        !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        memcmp(pixels, rendered, sizeof(pixels)) != 0) {
        ab_fail(error, "Metal clip/pixel command coordinates differ");
        goto release_renderer;
    }
    AbImage rejected = {.width = 19};
    const float clear_channels[] = {0, 0, 1, 1};
    if (ab_metal_capture(renderer, &list, 2, 2, clear_channels, 15, &rejected, NULL) ||
        rejected.width || rejected.height || rejected.rgba.data ||
        ab_metal_capture(renderer, &list, 0, 2, clear_channels, 16, &rejected, NULL) ||
        rejected.rgba.data) {
        ab_fail(error, "Metal capture rejected-budget/output checks failed");
        goto release_renderer;
    }
    const uint8_t expected[5][4] = {{64, 32, 16, 128},
                                    {64, 32, 143, 255},
                                    {32, 16, 135, 191},
                                    {64, 32, 255, 255},
                                    {0, 0, 16, 128}};
    for (AbBlend blend = AB_BLEND_OPAQUE; blend <= AB_BLEND_MULTIPLY; ++blend) {
        ab_draw_list_clear(&list);
        command.texture = 2;
        command.blend = blend;
        if (!ab_draw_list_push(&list, &command, error) ||
            !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
            !all_pixels((AbSpan){rendered, sizeof(rendered)}, expected[blend], error)) {
            goto release_renderer;
        }
    }
    ab_draw_list_clear(&list);
    command.blend = AB_BLEND_PREMULTIPLIED;
    command.opacity = 0.5f;
    const uint8_t faded[4] = {32, 16, 199, 255};
    if (!ab_draw_list_push(&list, &command, error) ||
        !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        !all_pixels((AbSpan){rendered, sizeof(rendered)}, faded, error)) {
        goto release_renderer;
    }
    ab_draw_list_clear(&list);
    command.color[1] = 0.5f;
    command.color[2] = 0.25f;
    const uint8_t tinted[4] = {32, 8, 193, 255};
    if (!ab_draw_list_push(&list, &command, error) ||
        !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
        !all_pixels((AbSpan){rendered, sizeof(rendered)}, tinted, error)) {
        goto release_renderer;
    }
    command.texture = 1;
    command.color[1] = 1;
    command.color[2] = 1;
    command.blend = AB_BLEND_OPAQUE;
    command.opacity = 1;
    if (!ab_draw_list_push(&list, &command, error) ||
        !render_pixels(device, renderer, format, &list, clear, rendered, error)) {
        goto release_renderer;
    }
    if (memcmp(pixels, rendered, sizeof(pixels)) != 0) {
        ab_fail(error, "Metal changed the order of overlapping commands");
        goto release_renderer;
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        ab_draw_list_clear(&list);
        if (!ab_metal_remove_texture(renderer, 1, error) ||
            !ab_metal_upload(renderer, 1, &image, repeat != 0, false, error)) {
            goto release_renderer;
        }
        for (size_t index = 0; index < 4; ++index) {
            command.quad.vertices[index].u = -0.25f;
            command.quad.vertices[index].v = 0.25f;
        }
        if (!ab_draw_list_push(&list, &command, error) ||
            !render_pixels(device, renderer, format, &list, clear, rendered, error) ||
            !all_pixels((AbSpan){rendered, sizeof(rendered)}, pixels + repeat * 4,
                        error)) {
            goto release_renderer;
        }
    }
    if (!ab_metal_remove_texture(renderer, 2, error) ||
        ab_metal_remove_texture(renderer, 2, NULL)) {
        ab_fail(error, "Metal texture removal lifetime check failed");
        goto release_renderer;
    }
    okay = true;

release_renderer:
    ab_draw_list_free(&list);
    ab_metal_free(renderer);
    return okay;
}

static bool gpu_rgb565_checks(id device, AbError *error) {
    AbMetal *renderer = NULL;
    AbDrawList list = {0};
    bool okay = false;
    if (!ab_metal_create(device, AB_METAL_RGB565, 1, 8192, &renderer, error) ||
        !ab_draw_list_init(1, 4096, &list, error)) {
        goto release_renderer;
    }
    uint8_t rendered[16];
    TestClearColor clear = {18.0 / 31, 50.0 / 63, 27.0 / 31, 0};
    const uint8_t sky[] = {148, 202, 222, 255};
    if (!render_pixels(device, renderer, AB_METAL_RGB565, &list, clear, rendered,
                       error) ||
        !all_pixels((AbSpan){rendered, sizeof(rendered)}, sky, error)) {
        goto release_renderer;
    }
    uint8_t pixels[] = {255, 0, 0, 128};
    AbImage image = {1, 1, {pixels, sizeof(pixels)}};
    if (!ab_metal_upload(renderer, 1, &image, false, false, error)) {
        goto release_renderer;
    }
    AbDrawCommand command = {
        .texture = 1,
        .blend = AB_BLEND_STRAIGHT,
        .color = {1, 1, 1, 1},
        .opacity = 1,
        .coordinates = AB_DRAW_CLIP,
        .quad = {{{-1, 1, 0, 0}, {1, 1, 1, 0}, {-1, -1, 0, 1}, {1, -1, 1, 1}}}};
    clear = (TestClearColor){0, 0, 1, 1};
    const uint8_t blended[] = {132, 0, 123, 255};
    if (!ab_draw_list_push(&list, &command, error) ||
        !render_pixels(device, renderer, AB_METAL_RGB565, &list, clear, rendered,
                       error) ||
        !all_pixels((AbSpan){rendered, sizeof(rendered)}, blended, error)) {
        goto release_renderer;
    }
    okay = true;

release_renderer:
    ab_draw_list_free(&list);
    ab_metal_free(renderer);
    return okay;
}

int main(int argc, char **argv) {
    bool rgb565 = argc == 2 && strcmp(argv[1], "--rgb565") == 0;
    if (argc != 1 && !rgb565) {
        return 2;
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    id device = MTLCreateSystemDefaultDevice();
    if (!device) {
        id devices = MTLCopyAllDevices();
        uintptr_t count =
            ((uintptr_t (*)(id, SEL))objc_msgSend)(devices, sel_registerName("count"));
        if (count) {
            device = ab_apple_retain(((id(*)(id, SEL, uintptr_t))objc_msgSend)(
                devices, sel_registerName("objectAtIndex:"), 0));
        }
        ab_apple_release(devices);
    }
    if (!device) {
        puts("Metal GPU unavailable; native execution was not validated.");
        ab_apple_release(pool);
        return 77;
    }
    AbError error = {0};
    bool okay = gpu_sprite_checks(device, AB_METAL_RGBA8, &error) &&
                gpu_sprite_checks(device, AB_METAL_BGRA8, &error) &&
                (!rgb565 || gpu_rgb565_checks(device, &error));
    ab_apple_release(device);
    ab_apple_release(pool);
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    puts("Native Metal RGBA8/BGRA8 orientation, blend, opacity, tint, wrapping, and "
         "command-order checks passed.");
    if (rgb565) {
        puts("Native Metal RGB565 clear conversion, opaque alpha, channel packing and "
             "straight-alpha blend checks passed.");
    }
    return 0;
}
