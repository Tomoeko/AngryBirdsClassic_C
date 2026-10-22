#include "android_io.h"
#include "../../tests/vp8_predict_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint32_t word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[161 * 9];
    if (!count || count > 161) {
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

void android_vp8_predict_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void);
    *(void **)&initialize = dlsym(library, "VP8DspInit");
    void **tables[3] = {dlsym(library, "VP8PredLuma16"),
                        dlsym(library, "VP8PredChroma8"),
                        dlsym(library, "VP8PredLuma4")};
    void *decode_mb = dlsym(library, "VP8DecodeMB");
    if (!initialize || !tables[0] || !tables[1] || !tables[2] || !decode_mb) {
        system_exit(2);
    }
    initialize();
    uintptr_t base = (uintptr_t)decode_mb - 0x878d88;
    static const char header[] = "AB_ANDROID_VP8_PREDICT_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t pixels[640];
        uint32_t input[3], output[161];
        vp8_predict_case(sample, input, pixels);
        print_words(input, 3);
        void (*predict)(uint8_t *);
        *(void **)&predict = tables[input[0]][input[1]];
        if (!predict) {
            system_exit(2);
        }
        predict(pixels + 72);
        output[0] = (uint32_t)((uintptr_t)tables[input[0]][input[1]] - base);
        for (size_t index = 0; index < 160; ++index) {
            output[index + 1] = word(pixels + index * 4);
        }
        print_words(output, 161);
    }
    static const char complete[] = "AB VP8 predictor reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
