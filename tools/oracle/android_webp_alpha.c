#include "android_io.h"
#include "../../tests/webp_alpha_cases.h"

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

void android_webp_alpha_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    typedef void (*Unfilter)(int, int, int, int, int, uint8_t *);
    Unfilter *filters = dlsym(library, "WebPUnfilters");
    if (!filters || filters[0] || !filters[1] || !filters[2] || !filters[3]) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_WEBP_ALPHA_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[6], output[160];
        uint8_t pixels[640];
        size_t size = webp_alpha_case(sample, input, pixels);
        print_words(input, 6);
        if (input[3]) {
            Unfilter filter = filters[input[3]];
            if (input[4]) {
                filter((int)input[0], (int)input[1], (int)input[2], 0, (int)input[4],
                       pixels);
            }
            if (input[4] < input[1]) {
                filter((int)input[0], (int)input[1], (int)input[2], (int)input[4],
                       (int)(input[1] - input[4]), pixels);
            }
        }
        for (size_t word = 0; word < (size + 3) / 4; ++word) {
            uint32_t value = 0;
            for (size_t byte = 0; byte < 4 && word * 4 + byte < size; ++byte) {
                value |= (uint32_t)pixels[word * 4 + byte] << (byte * 8);
            }
            output[word] = value;
        }
        print_words(output, (size + 3) / 4);
    }
    static const char complete[] = "AB WebP alpha reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
