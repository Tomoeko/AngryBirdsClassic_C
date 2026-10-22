#include "classic_preview_assets.h"
#include "gpu_scene_data.h"
#include "ab/hash.h"
#include "ab/metal.h"
#include "../src/platform/apple/objects.h"

#include <stdio.h>

extern id MTLCreateSystemDefaultDevice(void);
extern id MTLCopyAllDevices(void);

static id capture_device(void) {
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
    return device;
}

static bool upload(AbMetal *renderer, const PreviewLevel *level, AbError *error) {
    size_t count = level_preview_texture_count(level);
    for (size_t index = 0; index < count; ++index) {
        const AbImage *image;
        uint32_t texture;
        if (!level_preview_texture(level, index, &texture, &image) ||
            !ab_metal_upload(renderer, texture, image, false, false, error)) {
            return false;
        }
    }
    return true;
}

static bool upload_cached(AbMetal *renderer, const AbGpuSceneData *scene,
                          AbError *error) {
    for (uint32_t index = 0; index < scene->texture_count; ++index) {
        if (!ab_metal_upload(renderer, index + 1, scene->images + index, false, false,
                             error)) {
            return false;
        }
    }
    return true;
}

static bool write_ppm(const char *path, const AbImage *image, AbError *error) {
    FILE *file = fopen(path, "wbx");
    if (!file) {
        return ab_fail(error, "Cannot create new theme capture file");
    }
    bool okay = fprintf(file, "P6\n%u %u\n255\n", image->width, image->height) > 0;
    size_t pixels = (size_t)image->width * image->height;
    for (size_t index = 0; okay && index < pixels; ++index) {
        okay = fwrite(image->rgba.data + index * 4, 1, 3, file) == 3;
    }
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay || ab_fail(error, "Cannot write complete theme capture pixels");
}

int main(int argc, char **argv) {
    bool cached = argc > 1 && strcmp(argv[1], "--input") == 0;
    int first_option = cached ? 4 : 3;
    uint32_t scale = 1;
    AbMetalFormat format = AB_METAL_RGBA8;
    bool valid = argc >= first_option && argc <= first_option + 2;
    for (int index = first_option; valid && index < argc; ++index) {
        if (strcmp(argv[index], "2") == 0 && scale == 1) {
            scale = 2;
        } else if (strcmp(argv[index], "rgb565") == 0 && format == AB_METAL_RGBA8) {
            format = AB_METAL_RGB565;
        } else {
            valid = false;
        }
    }
    if (!valid) {
        fputs("Usage: ab_theme_capture APK NEW_OUTPUT.ppm [2] [rgb565]\n"
              "       ab_theme_capture --input PRIVATE_INPUT NEW_OUTPUT.ppm "
              "[2] [rgb565]\n",
              stderr);
        return 2;
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    id device = capture_device();
    if (!device) {
        fputs("Metal GPU unavailable; no theme pixels captured.\n", stderr);
        ab_apple_release(pool);
        return 77;
    }
    AbError error = {0};
    AbBuffer bytes = {0};
    AbZip archive = {0};
    PreviewLevel level = {0};
    AbGpuSceneData scene = {0};
    AbDrawList list = {0};
    const AbDrawList *frame = &list;
    AbMetal *renderer = NULL;
    AbImage image = {0};
    uint8_t digest[32];
    char hash[65];
    bool okay = ab_file_read(argv[cached ? 2 : 1],
                             (size_t)(cached ? 32 : 64) * 1024 * 1024, &bytes, &error);
    if (okay && !cached) {
        okay = ab_sha256((AbSpan){bytes.data, bytes.size}, digest, &error);
    }
    if (okay && !cached) {
        ab_digest_hex(digest, hash);
        okay =
            strcmp(
                hash,
                "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") ==
            0;
        if (!okay) {
            ab_fail(&error, "Theme capture requires the pinned APK");
        }
    }
    if (okay && cached) {
        okay = ab_gpu_scene_data_read((AbSpan){bytes.data, bytes.size}, &scene, &error);
        frame = &scene.list;
    } else if (okay) {
        okay = ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
               level_preview_open(&archive, &level, &error) &&
               ab_draw_list_init(PREVIEW_THEME_DRAWS + 32, 65536, &list, &error);
        for (size_t index = 0; okay && index < 128; ++index) {
            okay = level_preview_draw(&level, 0x1.111112p-5f, &list, NULL, &error);
        }
    }
    float clear[4];
    if (cached) {
        memcpy(clear, scene.clear, sizeof(clear));
    } else {
        theme_preview_clear(&level.theme, clear);
    }
    size_t textures =
        cached ? scene.texture_count : level_preview_texture_count(&level);
    okay = okay &&
           ab_metal_create(device, format, textures, 1024 * 1024, &renderer, &error) &&
           (cached ? upload_cached(renderer, &scene, &error)
                   : upload(renderer, &level, &error)) &&
           ab_metal_capture(renderer, frame, 480 * scale, 320 * scale, clear,
                            8 * 1024 * 1024, &image, &error) &&
           write_ppm(argv[cached ? 3 : 2], &image, &error);
    if (okay) {
        printf("Captured %u-by-%u %s %s (%zu commands).\n", image.width, image.height,
               format == AB_METAL_RGB565 ? "RGB565" : "RGBA8",
               cached ? "cached GPU input"
                      : "theme/object preview after 128 fixed updates",
               frame->count);
    } else {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_image_free(&image);
    ab_metal_free(renderer);
    ab_draw_list_free(&list);
    level_preview_free(&level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    ab_apple_release(device);
    ab_apple_release(pool);
    return okay ? 0 : 1;
}
