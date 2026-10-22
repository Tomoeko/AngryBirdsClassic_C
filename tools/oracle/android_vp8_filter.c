#include "android_io.h"
#include "../../tests/vp8_filter_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint32_t word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[513 * 9];
    if (!count || count > 513) {
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

void android_vp8_filter_main(void) {
    static const char *const names[12] = {
        "VP8VFilter16",       "VP8HFilter16",        "VP8VFilter16i",
        "VP8HFilter16i",      "VP8VFilter8",         "VP8HFilter8",
        "VP8VFilter8i",       "VP8HFilter8i",        "VP8SimpleVFilter16",
        "VP8SimpleHFilter16", "VP8SimpleVFilter16i", "VP8SimpleHFilter16i"};
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void);
    *(void **)&initialize = dlsym(library, "VP8DspInit");
    void *decode_mb = dlsym(library, "VP8DecodeMB");
    if (!initialize || !decode_mb) {
        system_exit(2);
    }
    initialize();
    void *functions[12];
    for (size_t index = 0; index < 12; ++index) {
        void **slot = dlsym(library, names[index]);
        if (!slot || !*slot) {
            system_exit(2);
        }
        functions[index] = *slot;
    }
    uintptr_t base = (uintptr_t)decode_mb - 0x878d88;
    static const char header[] = "AB_ANDROID_VP8_FILTER_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t pixels[2048];
        uint32_t input[5], output[513];
        vp8_filter_case(sample, input, pixels);
        print_words(input, 5);
        unsigned kind = input[0];
        if (kind >= 8) {
            void (*filter)(uint8_t *, int, int);
            *(void **)&filter = functions[kind];
            filter(pixels + 264, 32, (int)input[1]);
        } else if (kind >= 4) {
            void (*filter)(uint8_t *, uint8_t *, int, int, int, int);
            *(void **)&filter = functions[kind];
            filter(pixels + 264, pixels + 1288, 32, (int)input[1], (int)input[2],
                   (int)input[3]);
        } else {
            void (*filter)(uint8_t *, int, int, int, int);
            *(void **)&filter = functions[kind];
            filter(pixels + 264, 32, (int)input[1], (int)input[2], (int)input[3]);
        }
        output[0] = (uint32_t)((uintptr_t)functions[kind] - base);
        for (size_t index = 0; index < 512; ++index) {
            output[index + 1] = word(pixels + index * 4);
        }
        print_words(output, 513);
    }
    static const char complete[] = "AB VP8 filter reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
