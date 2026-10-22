#include "ab/rotation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    float output = 7;
    if (ab_angle_from_components(NAN, 1, &output) ||
        ab_angle_from_components(1, INFINITY, &output) || output != 7 ||
        ab_angle_from_components(1, 1, NULL) ||
        ab_script_angle_from_components(NAN, 1, &output) ||
        ab_script_angle_from_components(1, INFINITY, &output) || output != 7 ||
        ab_script_angle_from_components(1, 1, NULL)) {
        return false;
    }
    const uint32_t cases[][3] = {
        {0, 0, 0},
        {0x80000000, 0, 0x80000000},
        {0, 0x80000000, 0x40490fdb},
        {0x80000000, 0x80000000, 0xc0490fdb},
        {0x3f800000, 0, 0x3fc90fdb},
        {0xbf800000, 0, 0xbfc90fdb},
        {0x3f800000, 0x3f800000, 0x3f490fdb},
        {1, 0x3f800000, 1},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (!ab_angle_from_components(from_bits(cases[index][0]),
                                      from_bits(cases[index][1]), &output) ||
            to_bits(output) != cases[index][2]) {
            fprintf(stderr, "Angle edge %zu failed\n", index);
            return false;
        }
        if (!ab_script_angle_from_components(from_bits(cases[index][0]),
                                             from_bits(cases[index][1]), &output) ||
            to_bits(output) != cases[index][2]) {
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    uint32_t controls;
    if (!fgets(header, sizeof(header), file)) {
        return false;
    }
    bool script = strcmp(header, "AB_ANDROID_SCRIPT_ANGLE_V1 65536\n") == 0;
    if ((!script && strcmp(header, "AB_ANDROID_ANGLE_V1 65536\n") != 0) ||
        fscanf(file, "%" SCNx32 "\n", &controls) != 1 ||
        (controls & UINT32_C(0x03c00000))) {
        return false;
    }
    for (uint32_t index = 0; index < 65536; ++index) {
        uint32_t input[2], expected;
        float angle;
        if (fscanf(file, "%" SCNx32 " %" SCNx32 " %" SCNx32 "\n", input, input + 1,
                   &expected) != 3 ||
            !(script ? ab_script_angle_from_components(from_bits(input[0]),
                                                       from_bits(input[1]), &angle)
                     : ab_angle_from_components(from_bits(input[0]),
                                                from_bits(input[1]), &angle))) {
            return false;
        }
        if (to_bits(angle) != expected) {
            fprintf(stderr,
                    "Native angle %" PRIu32 " inputs %08" PRIx32 " %08" PRIx32
                    ": %08" PRIx32 " != %08" PRIx32 "\n",
                    index, input[0], input[1], to_bits(angle), expected);
            return false;
        }
    }
    if (!fgets(header, sizeof(header), file) ||
        strcmp(header, "AB angle reference complete\n") != 0 || fgetc(file) != EOF ||
        ferror(file)) {
        return false;
    }
    printf("All 65536 Android %s component results match.\n",
           script ? "Lua atan2 promotion/rounding" : "atan2f");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || !edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
