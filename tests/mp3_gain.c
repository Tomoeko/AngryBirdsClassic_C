#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 gain failed at %d: %s\n", __LINE__, #condition);      \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool native_checks(FILE *file) {
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_GAIN_V1 378\n") == 0);
    for (int quarter_steps = -46; quarter_steps <= 331; ++quarter_steps) {
        uint32_t index, expected, bits;
        CHECK(fscanf(file, "%" SCNx32 " %" SCNx32, &index, &expected) == 2 &&
              fgetc(file) == '\n' && index == (uint32_t)quarter_steps);
        float value;
        CHECK(ab_mp3_gain_power(quarter_steps, &value, NULL));
        memcpy(&bits, &value, sizeof(bits));
        CHECK(bits == expected);
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 gain reference complete\n") == 0 && fgetc(file) == EOF &&
          !ferror(file));
    puts("Compared all 378 native quarter-step gain factors");
    return true;
}

static bool edge_checks(void) {
    float value = -13.0f;
    CHECK(!ab_mp3_gain_power(-47, &value, NULL) && value == -13.0f &&
          !ab_mp3_gain_power(332, &value, NULL) && value == -13.0f &&
          !ab_mp3_gain_power(0, NULL, NULL));
    CHECK(ab_mp3_gain_power(0, &value, NULL) && value == 1.0f &&
          ab_mp3_gain_power(4, &value, NULL) && value == 0.5f &&
          ab_mp3_gain_power(-4, &value, NULL) && value == 2.0f);
    float previous = 4096.0f;
    for (int quarter_steps = -46; quarter_steps <= 331; ++quarter_steps) {
        CHECK(ab_mp3_gain_power(quarter_steps, &value, NULL) && value > 0.0f &&
              value < previous);
        previous = value;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_mp3_gain_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
