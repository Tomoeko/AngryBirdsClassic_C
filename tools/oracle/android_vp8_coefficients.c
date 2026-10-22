#include "android_io.h"
#include "../../tests/vp8_coefficient_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[14 * 9];
    if (!count || count > 14) {
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

void android_vp8_coefficients_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*initialize)(void *, const uint8_t *, const uint8_t *);
    int (*coefficients)(void *, const uint8_t *, int, const int32_t *, int, int16_t *);
    void *decode_mb = dlsym(library, "VP8DecodeMB");
    *(void **)&initialize = dlsym(library, "VP8InitBitReader");
    if (!decode_mb || !initialize) {
        system_exit(2);
    }
    *(void **)&coefficients = (void *)((uintptr_t)decode_mb - 0x878d88 + 0x8778d0);
    static const char header[] = "AB_ANDROID_VP8_COEFFICIENTS_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t bytes[8192], probabilities[264];
        uint32_t input[13], reader[6];
        vp8_coefficient_case(sample, input, bytes, probabilities);
        print_words(input, 13);
        initialize(reader, bytes, bytes + input[0]);
        int32_t quantizers[2] = {(int32_t)input[3], (int32_t)input[4]};
        for (size_t block = 0; block < 8; ++block) {
            int16_t values[20] = {0};
            values[0] = values[1] = values[18] = values[19] = 12345;
            unsigned first = input[5 + block] & 1;
            unsigned context = input[5 + block] >> 1;
            if (first) {
                values[2] = -1234;
            }
            int end = coefficients(reader, probabilities, (int)context, quantizers,
                                   (int)first, values + 2);
            int32_t pending = (int32_t)reader[2];
            uintptr_t cursor = reader[3];
            int64_t consumed = (int64_t)(cursor - (uintptr_t)bytes) * 8 - pending - 8;
            if (end < 0 || end > 16 || reader[5] || cursor < (uintptr_t)bytes ||
                cursor > (uintptr_t)bytes + sizeof(bytes) || consumed < 0 ||
                (uint64_t)consumed > sizeof(bytes) * 8) {
                system_exit(4);
            }
            uint32_t output[14] = {(uint32_t)end, reader[1] + 1, (uint32_t)consumed,
                                   reader[5]};
            for (size_t index = 0; index < 10; ++index) {
                output[4 + index] = (uint32_t)(uint16_t)values[index * 2] |
                                    ((uint32_t)(uint16_t)values[index * 2 + 1] << 16);
            }
            print_words(output, 14);
        }
    }
    static const char complete[] = "AB VP8 coefficient reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
