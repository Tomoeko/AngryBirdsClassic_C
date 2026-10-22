#include "ab/metal.h"
#include "font_preview_assets.h"
#include "../src/platform/apple/objects.h"

extern id MTLCopyAllDevices(void);

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "Usage: ab_font_preview APK FONT_DAT_ENTRY UTF8_TEXT NEW_PPM\n");
        return 2;
    }
    int result = 1;
    AbError error = {0};
    AbBuffer apk = {0}, font_bytes = {0}, encoded = {0};
    AbZip zip = {0};
    AbBitmapFont font = {0};
    AbImage image = {0}, frame = {0};
    AbDrawList list = {0};
    AbMetal *renderer = NULL;
    id pool = ab_apple_new("NSAutoreleasePool");
    id devices = MTLCopyAllDevices();
    uintptr_t count =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(devices, sel_registerName("count"));
    id device = count ? ((id(*)(id, SEL, uintptr_t))objc_msgSend)(
                            devices, sel_registerName("objectAtIndex:"), 0)
                      : nil;
    if (!device) {
        ab_fail(&error, "Native Metal device is unavailable");
        goto release_preview;
    }
    if (!pinned_archive(argv[1], &apk, &zip, &error) ||
        !read_entry(&zip, argv[2], &font_bytes, &error) ||
        !ab_font_read((AbSpan){font_bytes.data, font_bytes.size}, 1024 * 1024, &font,
                      &error) ||
        !metadata_image(&zip, argv[2], font.texture, &encoded, &image, &error) ||
        !ab_draw_list_init(TEXT_LIMIT, 1024 * 1024, &list, &error) ||
        !build_text(&font, &image, argv[3], false, &list, &error) ||
        !ab_metal_create(device, AB_METAL_RGBA8, 1, 16384, &renderer, &error) ||
        !ab_metal_upload(renderer, 1, &image, false, false, &error)) {
        goto release_preview;
    }
    const float clear[] = {0.2f, 0.4f, 0.6f, 1};
    if (!ab_metal_capture(renderer, &list, PREVIEW_WIDTH, PREVIEW_HEIGHT, clear,
                          1024 * 1024, &frame, &error) ||
        !save_ppm(argv[4], &frame, &error)) {
        goto release_preview;
    }
    uint8_t digest[32];
    char hash[65];
    if (!ab_sha256((AbSpan){frame.rgba.data, frame.rgba.size}, digest, &error)) {
        goto release_preview;
    }
    ab_digest_hex(digest, hash);
    printf("Rendered %zu glyphs at 480x320; RGBA8 SHA-256 %s\n", list.count, hash);
    result = 0;

release_preview:
    if (result) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_metal_free(renderer);
    ab_draw_list_free(&list);
    ab_image_free(&frame);
    ab_image_free(&image);
    ab_font_free(&font);
    ab_buffer_free(&encoded);
    ab_buffer_free(&font_bytes);
    ab_zip_close(&zip);
    ab_buffer_free(&apk);
    ab_apple_release(devices);
    ab_apple_release(pool);
    return result;
}
