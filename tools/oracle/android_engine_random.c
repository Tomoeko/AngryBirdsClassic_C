#include "android_io.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *symbol);

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[54];
    if (count != 6)
        system_exit(2);
    for (size_t index = 0; index < count; ++index) {
        for (unsigned digit = 0; digit < 8; ++digit)
            line[index * 9 + digit] = digits[(words[index] >> (28 - digit * 4)) & 15];
        line[index * 9 + 8] = index + 1 == count ? '\n' : ' ';
    }
    write_bytes(line, sizeof(line));
}

void android_engine_random_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library)
        system_exit(1);
    void (*seed)(uint32_t) =
        (void (*)(uint32_t))dlsym(library, "_ZN4math10RandomUtil7setSeedEj");
    double (*next)(void) =
        (double (*)(void))dlsym(library, "_ZN4math10RandomUtil6randomEv");
    if (!seed || !next)
        system_exit(3);
    uint32_t controls = 0;
    __asm__ volatile("vmsr fpscr, %0" : : "r"(controls) : "memory");
    static const char header[] = "AB_ANDROID_ENGINE_RANDOM_V1 4 4112\n";
    write_bytes(header, sizeof(header) - 1);
    const uint32_t seeds[] = {123456789, 0, 1, UINT32_MAX};
    for (uint32_t mode = 0; mode < 4; ++mode) {
        if (mode)
            seed(seeds[mode]);
        for (uint32_t draw = 0; draw < 4112; ++draw) {
            union {
                double number;
                uint64_t bits;
            } value = {.number = next()};
            union {
                float number;
                uint32_t bits;
            } rounded = {.number = (float)value.number};
            if (draw < 8 || draw >= 4088) {
                uint32_t words[] = {mode,
                                    seeds[mode],
                                    draw,
                                    (uint32_t)value.bits,
                                    (uint32_t)(value.bits >> 32),
                                    rounded.bits};
                print_words(words, 6);
            }
        }
    }
    static const char marker[] = "AB engine random reference complete\n";
    write_bytes(marker, sizeof(marker) - 1);
    system_exit(0);
}
