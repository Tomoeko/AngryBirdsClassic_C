#include "android_io.h"
#include "../../tests/resource_path_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

void __aeabi_memclr8(void *destination, size_t size) {
    volatile uint8_t *bytes = destination;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[129 * 9];
    if (!count || count > 129) {
        system_exit(3);
    }
    for (size_t word = 0; word < count; ++word) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[word * 9 + digit] = digits[(words[word] >> (28 - digit * 4)) & 15];
        }
        line[word * 9 + 8] = word + 1 < count ? ' ' : '\n';
    }
    write_bytes(line, count * 9);
}

void android_resource_path_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*construct)(void *, const void *);
    *(void **)&construct = dlsym(library, "_ZN2io8PathNameC2ERKSs");
    if (!construct) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_RESOURCE_PATH_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        char path[256];
        size_t size = resource_path_case(sample, path);
        uint32_t input[2] = {(uint32_t)size, resource_path_hash(path, size)};
        print_words(input, 2);
        uint64_t storage[192] = {0};
        const char *argument = path;
        construct(storage, &argument);
        /* Resources create/release reads PathName's basename pointer at +12,
         * then parses that basename again before using its own basename as key. */
        const char *basename = ((const char *const *)(const void *)storage)[3];
        uintptr_t start = (uintptr_t)storage;
        if ((uintptr_t)basename < start ||
            (uintptr_t)basename >= start + sizeof(storage)) {
            system_exit(4);
        }
        uint64_t second[192] = {0};
        construct(second, &basename);
        const char *key = ((const char *const *)(const void *)second)[3];
        start = (uintptr_t)second;
        if ((uintptr_t)key < start || (uintptr_t)key >= start + sizeof(second)) {
            system_exit(4);
        }
        uint32_t output[129] = {0};
        while (key[output[0]]) {
            if (output[0] == 128) {
                system_exit(5);
            }
            output[output[0] + 1] = (uint8_t)key[output[0]];
            ++output[0];
        }
        print_words(output, output[0] + 1);
    }
    static const char complete[] = "AB resource path reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
