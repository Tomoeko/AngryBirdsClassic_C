#include "ab/random.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Engine random check at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool controls(void) {
    AbEngineRandom state = {0};
    double value = 42;
    CHECK(!ab_engine_random_seed(NULL, 0) && !ab_engine_random_next(NULL, &value) &&
          !ab_engine_random_next(&state, NULL) && !state.initialized && value == 42);
    state.initialized = true;
    state.cursor = 4096;
    CHECK(!ab_engine_random_next(&state, &value) && state.cursor == 4096 &&
          value == 42);
    return true;
}

static bool native_stream(FILE *file) {
    char header[64];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_ENGINE_RANDOM_V1 4 4112\n"));
    const uint32_t seeds[] = {123456789, 0, 1, UINT32_MAX};
    AbEngineRandom state = {0};
    size_t compared = 0;
    for (uint32_t mode = 0; mode < 4; ++mode) {
        if (mode)
            CHECK(ab_engine_random_seed(&state, seeds[mode]));
        for (uint32_t draw = 0; draw < 4112; ++draw) {
            double value;
            CHECK(ab_engine_random_next(&state, &value));
            if (draw < 8 || draw >= 4088) {
                uint32_t expected[6];
                for (size_t word = 0; word < 6; ++word)
                    CHECK(fscanf(file, "%" SCNx32, expected + word) == 1);
                uint64_t bits;
                memcpy(&bits, &value, sizeof(bits));
                float rounded = (float)value;
                uint32_t rounded_bits;
                memcpy(&rounded_bits, &rounded, sizeof(rounded_bits));
                uint32_t actual[] = {mode,
                                     seeds[mode],
                                     draw,
                                     (uint32_t)bits,
                                     (uint32_t)(bits >> 32),
                                     rounded_bits};
                if (memcmp(actual, expected, sizeof(actual))) {
                    fprintf(stderr,
                            "Engine native stream mode %u draw %u: "
                            "%08" PRIx32 " %08" PRIx32 " %08" PRIx32 " != %08" PRIx32
                            " %08" PRIx32 " %08" PRIx32 "\n",
                            mode, draw, actual[3], actual[4], actual[5], expected[3],
                            expected[4], expected[5]);
                    return false;
                }
                ++compared;
            }
        }
    }
    char marker[64];
    CHECK(fscanf(file, "\n%63[^\n]", marker) == 1 &&
          !strcmp(marker, "AB engine random reference complete"));
    int trailing;
    while ((trailing = fgetc(file)) != EOF)
        CHECK(trailing == '\n' || trailing == '\r');
    CHECK(!ferror(file) && compared == 128);
    printf("Engine RNG matches 128 native binary64/float checkpoints across "
           "default/explicit seeds and the 4096-word wrap\n");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("Usage: ab_engine_random_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    bool okay = controls();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        okay = file && native_stream(file);
        if (file && fclose(file))
            okay = false;
    }
    return okay ? 0 : 1;
}
