#include "ab/font_metrics.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Font metrics failed at line %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static int32_t signed_word(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value
                              : (int32_t)((int64_t)value - INT64_C(4294967296));
}

static int16_t signed_half(uint32_t value) {
    value &= UINT32_C(0xffff);
    return (int16_t)(value < 32768 ? (int32_t)value : (int32_t)value - 65536);
}

static bool edge_checks(void) {
    AbBitmapFont font = {
        .tracking = 5, .leading = 11, .max_ascending = 8, .max_descending = 3};
    AbFontMeasure result;
    const uint32_t code = 'A';
    CHECK(ab_font_measure_codes(&font, NULL, 0, 0, 0, &result));
    CHECK(result.width == 0 && result.height == 0 && result.font_height == 11);
    CHECK(ab_font_measure_codes(&font, &code, 1, 0, 0, &result));
    CHECK(result.width == -5 && result.height == 0);
    CHECK(ab_font_measure_codes(&font, &code, 1, 0, 1, &result));
    CHECK(result.width == 3 && result.height == 0);
    result.width = 19;
    font.max_ascending = INT32_MAX;
    CHECK(!ab_font_measure_codes(&font, &code, 1, 0, 1, &result));
    CHECK(result.width == 19);
    font.count = 1;
    CHECK(!ab_font_measure_codes(&font, &code, 1, 0, 1, &result));
    font = (AbBitmapFont){0};
    CHECK(!ab_font_measure_codes(&font, NULL, 1, 0, 1, &result));
    CHECK(!ab_font_measure_codes(&font, NULL, 0, SIZE_MAX, 0, &result));
    CHECK(!ab_font_measure_codes(NULL, NULL, 0, 0, 0, &result));
    CHECK(!ab_font_measure_codes(&font, NULL, 0, 0, 0, NULL));
    AbFontGlyph glyph = {.code = 'A', .width = -1, .height = -2};
    font.glyphs = &glyph;
    font.count = 1;
    CHECK(ab_font_measure_codes(&font, &code, 1, 0, 1, &result));
    CHECK(result.width == -1 && result.height == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_FONT_METRICS_V1 4096\n") == 0);
    static const uint32_t codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    size_t empty_ranges = 0, negative_widths = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[31], expected[3];
        CHECK(read_words(file, input, 31) && read_words(file, expected, 3));
        CHECK(input[0] <= 7 && input[5] <= 16 && input[6] <= 19 && input[7] <= 23);
        int32_t tracking = signed_word(input[1]);
        CHECK(tracking >= INT16_MIN && tracking <= INT16_MAX);
        AbFontGlyph glyphs[7];
        for (size_t index = 0; index < 7; ++index) {
            glyphs[index] =
                (AbFontGlyph){.code = codes[index],
                              .width = signed_half(input[8 + index]),
                              .height = signed_half(input[8 + index] >> 16)};
        }
        AbBitmapFont font = {.glyphs = glyphs,
                             .count = input[0],
                             .tracking = (int16_t)tracking,
                             .leading = signed_half(input[4]),
                             .max_ascending = signed_word(input[2]),
                             .max_descending = signed_word(input[3])};
        AbFontMeasure result;
        CHECK(ab_font_measure_codes(&font, input + 15, input[5], input[6], input[7],
                                    &result));
        const uint32_t actual[] = {(uint32_t)result.width, (uint32_t)result.height,
                                   (uint32_t)result.font_height};
        for (size_t field = 0; field < 3; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native font metrics %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        empty_ranges += input[5] == 0 || input[6] >= input[5] || input[7] == 0;
        negative_widths += result.width < 0;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB font metrics reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native UTF-32 font range metrics match: %zu empty ranges, "
           "%zu negative widths.\n",
           empty_ranges, negative_widths);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_font_metrics_tests [NATIVE_FONT_METRIC_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
