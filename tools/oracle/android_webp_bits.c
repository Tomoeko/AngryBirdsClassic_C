#include "android_io.h"
#include "../../tests/webp_bits_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[34 * 9];
    if (!count || count > 34) {
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

void android_webp_bits_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void *, const uint8_t *, size_t);
    uint32_t (*read_bits)(void *, int);
    *(void **)&initialize = dlsym(library, "VP8LInitBitReader");
    *(void **)&read_bits = dlsym(library, "VP8LReadBits");
    if (!initialize || !read_bits) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_WEBP_BITS_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint64_t reader[16];
        uint8_t bytes[512];
        uint32_t input[34], output[32];
        webp_bits_case(sample, input, bytes);
        print_words(input, 34);
        initialize(reader, bytes, input[0]);
        for (size_t index = 0; index < 32; ++index) {
            output[index] = read_bits(reader, (int)input[index + 2]);
        }
        print_words(output, 32);
    }
    static const char complete[] = "AB WebP bits reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
