#include "ab/mp3.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 windows failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float number) {
    uint32_t value;
    memcpy(&value, &number, sizeof(value));
    return value;
}

static bool edges(void) {
    AbMp3HybridWindows windows;
    CHECK(ab_mp3_hybrid_windows_init(&windows, NULL) &&
          !ab_mp3_hybrid_windows_init(NULL, NULL));
    for (size_t parity = 0; parity < 2; ++parity) {
        for (size_t block = 0; block < 4; ++block) {
            for (size_t index = 0; index < 36; ++index) {
                CHECK(isfinite(windows.values[parity][block][index]));
                if (block == 2 && index >= 12) {
                    CHECK(bits(windows.values[parity][block][index]) == 0);
                }
                uint32_t expected = bits(windows.values[0][block][index]);
                if (parity && (index & 1u) && (block != 2 || index < 12)) {
                    expected ^= UINT32_C(0x80000000);
                }
                CHECK(bits(windows.values[parity][block][index]) == expected);
            }
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_WINDOWS_V1 288\n") == 0);
    AbMp3HybridWindows windows;
    CHECK(ab_mp3_hybrid_windows_init(&windows, NULL));
    for (size_t parity = 0; parity < 2; ++parity) {
        for (size_t block = 0; block < 4; ++block) {
            uint32_t expected_parity, expected_block;
            CHECK(fscanf(file, "%" SCNx32 " %" SCNx32, &expected_parity,
                         &expected_block) == 2 &&
                  fgetc(file) == '\n' && expected_parity == parity &&
                  expected_block == block);
            for (size_t index = 0; index < 36; ++index) {
                uint32_t expected;
                CHECK(fscanf(file, "%" SCNx32, &expected) == 1 &&
                      fgetc(file) == (index == 35 ? '\n' : ' '));
                uint32_t actual = bits(windows.values[parity][block][index]);
                if (actual != expected) {
                    fprintf(stderr,
                            "Window parity %zu block %zu index %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            parity, block, index, actual, expected);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 windows reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared all 288 native hybrid-window float coefficients\n");
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_mp3_windows_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
