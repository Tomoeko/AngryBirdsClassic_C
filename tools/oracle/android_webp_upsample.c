#include "android_io.h"
#include "../../tests/webp_upsample_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[160 * 9];
    if (!count || count > 160) {
        system_exit(3);
    }
    for (size_t word = 0; word < count; ++word) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[word * 9 + digit] = digits[(words[word] >> (28 - digit * 4)) & 15];
        }
        line[word * 9 + 8] = word + 1 == count ? '\n' : ' ';
    }
    write_bytes(line, count * 9);
}

void android_webp_upsample_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void), (*initialize_yuv)(void);
    *(void **)&initialize = dlsym(library, "WebPInitUpsamplers");
    *(void **)&initialize_yuv = dlsym(library, "VP8YUVInit");
    void **table = dlsym(library, "WebPUpsamplers");
    if (!initialize || !initialize_yuv || !table) {
        system_exit(2);
    }
    initialize_yuv();
    initialize();
    typedef void (*Upsample)(const uint8_t *, const uint8_t *, const uint8_t *,
                             const uint8_t *, const uint8_t *, const uint8_t *,
                             uint8_t *, uint8_t *, int);
    Upsample convert;
    *(void **)&convert = table[1];
    if (!convert) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_WEBP_UPSAMPLE_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    uint32_t selected = (uint32_t)((uintptr_t)table[1] - (uintptr_t)table) + 0xbdaf64;
    print_words(&selected, 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[4], output[160];
        WebpUpsampleCase planes;
        uint8_t top[320], bottom[320];
        webp_upsample_case(sample, input, &planes);
        for (size_t index = 0; index < 320; ++index) {
            top[index] = bottom[index] = 0xa5;
        }
        print_words(input, 4);
        convert(input[1] & 1u ? planes.top_y : NULL,
                input[1] & 2u ? planes.bottom_y : NULL, planes.top_u, planes.top_v,
                planes.bottom_u, planes.bottom_v, input[1] & 1u ? top : NULL,
                input[1] & 2u ? bottom : NULL, (int)input[0]);
        for (size_t word = 0; word < 160; ++word) {
            uint32_t value = 0;
            const uint8_t *bytes = word < 80 ? top : bottom;
            size_t offset = (word % 80) * 4;
            for (size_t byte = 0; byte < 4; ++byte) {
                value |= (uint32_t)bytes[offset + byte] << (byte * 8);
            }
            output[word] = value;
        }
        print_words(output, 160);
    }
    static const char complete[] = "AB WebP upsample reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
