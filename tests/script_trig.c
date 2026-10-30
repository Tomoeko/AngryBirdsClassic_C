#include "ab/script_math.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Script trig failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    float value = 99;
    CHECK(ab_script_sine(-0.0f, &value) && bits(value) == UINT32_C(0x80000000));
    CHECK(ab_script_cosine(-0.0f, &value) && value == 1);
    CHECK(ab_script_cosine(0x1.921fb6p+0f, &value) &&
          bits(value) == UINT32_C(0xb33bbd2e));
    CHECK(ab_script_cosine(1024, &value) && ab_script_cosine(-1024, &value));
    value = 99;
    CHECK(!ab_script_cosine(1025, &value) && value == 99 &&
          !ab_script_cosine(NAN, &value) && value == 99 &&
          !ab_script_sine(INFINITY, &value) && value == 99 &&
          !ab_script_cosine(0, NULL));
    puts("Script sine/cosine signed zero, quadrant boundary and finite-domain checks "
         "pass.");
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_SCRIPT_TRIG_V1 131072\n") == 0);
    for (size_t item = 0; item < 131072; ++item) {
        uint32_t input, sine, cosine;
        CHECK(fscanf(file, "%" SCNx32 " %" SCNx32 " %" SCNx32, &input, &sine,
                     &cosine) == 3 &&
              fgetc(file) == '\n');
        float actual_sine, actual_cosine = 0.0f;
        CHECK(ab_script_sine(number(input), &actual_sine) &&
              ab_script_cosine(number(input), &actual_cosine));
        if (bits(actual_sine) != sine || bits(actual_cosine) != cosine) {
            fprintf(stderr,
                    "Script trig case %zu angle %08" PRIx32 ": "
                    "%08" PRIx32 "/%08" PRIx32 " != %08" PRIx32 "/%08" PRIx32 "\n",
                    item, input, bits(actual_sine), bits(actual_cosine), sine, cosine);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB script trigonometry reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 131072 original float Lua math.sin/math.cos pairs match first-party "
         "range-reduced polynomial evaluation.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && compare_native(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
