#include "../src/resource/vp8_filter.h"
#include "ab/base.h"
#include "vp8_filter_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 filter check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static void filter_case(const uint32_t input[5], uint8_t pixels[2048]) {
    unsigned kind = input[0];
    bool chroma = kind >= 4 && kind < 8;
    bool internal = kind == 2 || kind == 3 || kind == 6 || kind == 7 || kind >= 10;
    bool simple = kind >= 8;
    size_t normal = kind % 2 ? 1 : 32;
    size_t along = kind % 2 ? 32 : 1;
    size_t plane_count = chroma ? 2 : 1;
    size_t edge_count = internal ? (chroma ? 1 : 3) : 1;
    for (size_t plane = 0; plane < plane_count; ++plane) {
        for (size_t edge = 0; edge < edge_count; ++edge) {
            size_t offset = internal ? (edge + 1) * 4 * normal : 0;
            ab_vp8_filter_edge(pixels + plane * 1024 + 264 + offset, normal, along,
                               chroma ? 8 : 16, (uint8_t)input[1], (uint8_t)input[2],
                               (uint8_t)input[3], !internal, simple);
        }
    }
}

static bool edge_checks(void) {
    uint8_t pixels[2048];
    for (unsigned kind = 0; kind < 12; ++kind) {
        memset(pixels, 127, sizeof(pixels));
        uint32_t input[5] = {kind, 127, 63, 7, 0};
        filter_case(input, pixels);
        for (size_t index = 0; index < sizeof(pixels); ++index) {
            CHECK(pixels[index] == 127);
        }
    }
    ab_vp8_filter_edge(NULL, 1, 1, 0, 0, 0, 0, false, false);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool reference_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_FILTER_V1 4096\n") == 0);
    uint32_t selected[12] = {0};
    size_t changed[12] = {0};
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[5], expected_input[5], expected[513];
        uint8_t pixels[2048], before[2048];
        vp8_filter_case(sample, input, pixels);
        memcpy(before, pixels, sizeof(before));
        CHECK(read_words(file, expected_input, 5) &&
              memcmp(input, expected_input, sizeof(input)) == 0 &&
              read_words(file, expected, 513));
        unsigned kind = input[0];
        CHECK(expected[0] && (!selected[kind] || selected[kind] == expected[0]));
        selected[kind] = expected[0];
        filter_case(input, pixels);
        for (size_t index = 0; index < 512; ++index) {
            uint32_t actual = ab_u32le(pixels + index * 4);
            if (actual != expected[index + 1]) {
                fprintf(stderr,
                        "VP8 filter case %zu type %u word %zu: %08" PRIx32 "/%08" PRIx32
                        "\n",
                        sample, kind, index, actual, expected[index + 1]);
                return false;
            }
        }
        changed[kind] += memcmp(pixels, before, sizeof(before)) != 0;
    }
    for (size_t kind = 0; kind < 12; ++kind) {
        CHECK(selected[kind] && changed[kind]);
        printf("VP8 filter %zu: %zu cases changed pixels.\n", kind, changed[kind]);
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 filter reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched all 4,096 original loop filter calls and all pixel/guard bytes.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = reference_checks(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
