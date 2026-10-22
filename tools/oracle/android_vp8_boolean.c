#include "android_io.h"
#include "../../tests/vp8_boolean_cases.h"

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

void android_vp8_boolean_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    /* The original VP8 initializer takes an end pointer, unlike VP8L's size. */
    void (*initialize)(void *, const uint8_t *, const uint8_t *);
    uint32_t (*read_value)(void *, int);
    int32_t (*read_signed)(void *, int);
    *(void **)&initialize = dlsym(library, "VP8InitBitReader");
    *(void **)&read_value = dlsym(library, "VP8GetValue");
    *(void **)&read_signed = dlsym(library, "VP8GetSignedValue");
    if (!initialize || !read_value || !read_signed) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_VP8_BOOLEAN_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint64_t reader[16];
        uint8_t bytes[512];
        uint32_t input[34], output[32];
        vp8_boolean_case(sample, input, bytes);
        print_words(input, 34);
        initialize(reader, bytes, bytes + input[0]);
        for (size_t index = 0; index < 32; ++index) {
            uint32_t command = input[index + 2];
            int width = (int)(command & 31u);
            output[index] = command & 32u ? (uint32_t)read_signed(reader, width)
                                          : read_value(reader, width);
        }
        print_words(output, 32);
    }
    static const char complete[] = "AB VP8 boolean reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
