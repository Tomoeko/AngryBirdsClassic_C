#include "android_io.h"
#include "../../tests/vp8_transform_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint32_t word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[133 * 9];
    if (!count || count > 133) {
        system_exit(3);
    }
    for (size_t index = 0; index < count; ++index) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[index * 9 + digit] = digits[(words[index] >> (28 - digit * 4)) & 15];
        }
        line[index * 9 + 8] = index + 1 == count ? '\n' : ' ';
    }
    write_bytes(line, count * 9);
}

void android_vp8_transform_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void);
    void (*walsh)(const int16_t *, int16_t *);
    void (*transform)(const int16_t *, uint8_t *, int);
    void (*dc)(const int16_t *, uint8_t *);
    void (*ac3)(const int16_t *, uint8_t *);
    *(void **)&initialize = dlsym(library, "VP8DspInit");
    void **walsh_slot = dlsym(library, "VP8TransformWHT");
    void **transform_slot = dlsym(library, "VP8Transform");
    void **dc_slot = dlsym(library, "VP8TransformDC");
    void **ac3_slot = dlsym(library, "VP8TransformAC3");
    void *decode_mb = dlsym(library, "VP8DecodeMB");
    if (!initialize || !walsh_slot || !transform_slot || !dc_slot || !ac3_slot ||
        !decode_mb) {
        system_exit(2);
    }
    initialize();
    *(void **)&walsh = *walsh_slot;
    *(void **)&transform = *transform_slot;
    *(void **)&dc = *dc_slot;
    *(void **)&ac3 = *ac3_slot;
    if (!walsh || !transform || !dc || !ac3) {
        system_exit(2);
    }
    uintptr_t base = (uintptr_t)decode_mb - 0x878d88;
    static const char header[] = "AB_ANDROID_VP8_TRANSFORM_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        int16_t values[32];
        union {
            uint64_t alignment;
            uint8_t bytes[528];
            int16_t shorts[264];
        } pixels;
        uint32_t input[3], output[133];
        vp8_transform_case(sample, input, values, pixels.bytes);
        print_words(input, 3);
        unsigned kind = input[0];
        if (kind == 0) {
            walsh(values, pixels.shorts + 4);
            output[0] = (uint32_t)((uintptr_t)*walsh_slot - base);
        } else if (kind <= 2) {
            transform(values, pixels.bytes + 72, kind == 2);
            output[0] = (uint32_t)((uintptr_t)*transform_slot - base);
        } else if (kind == 3) {
            dc(values, pixels.bytes + 72);
            output[0] = (uint32_t)((uintptr_t)*dc_slot - base);
        } else {
            ac3(values, pixels.bytes + 72);
            output[0] = (uint32_t)((uintptr_t)*ac3_slot - base);
        }
        for (size_t index = 0; index < 132; ++index) {
            output[index + 1] = word(pixels.bytes + index * 4);
        }
        print_words(output, 133);
    }
    static const char complete[] = "AB VP8 transform reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
