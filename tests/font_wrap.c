#include "ab/font_wrap.h"
#include "ab/utf8.h"
#include "../tools/oracle/ui_text_wrap_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Font wrapping failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t raw(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbFontGlyph glyphs[] = {{.code = ' ', .width = 8},
                            {.code = 'A', .width = 10},
                            {.code = 'B', .width = 11}};
    AbBitmapFont font = {.glyphs = glyphs, .count = 3, .tracking = 1, .leading = 30};
    AbFontWrappedLine lines[8];
    AbFontWrap result = {.lines = lines, .capacity = 8};
    const uint32_t pair[] = {'A', 'B'}, spaced[] = {'A', ' ', 'B'};
    CHECK(ab_font_wrap_codes(&font, pair, 2, 23, &result, NULL) && result.count == 1 &&
          result.widest_line == 22 && lines[0].count == 2);
    CHECK(ab_font_wrap_codes(&font, pair, 2, 22, &result, NULL) && result.count == 2 &&
          result.widest_line == 11 && lines[0].count == 1 && lines[1].start == 1);
    CHECK(ab_font_wrap_codes(&font, spaced, 3, 22, &result, NULL) &&
          result.count == 2 && lines[0].count == 1 && lines[1].start == 2);
    const uint32_t leading[] = {' ', 'A'}, trailing[] = {'A', ' '};
    CHECK(ab_font_wrap_codes(&font, leading, 2, 16, &result, NULL) &&
          result.count == 2 && lines[0].count == 0 && lines[0].width == -1 &&
          lines[1].start == 1);
    CHECK(ab_font_wrap_codes(&font, trailing, 2, 16, &result, NULL) &&
          result.count == 1 && lines[0].count == 1);
    const uint32_t hyphen[] = {'A', '-', 'B'}, blank[] = {'\n', '\n'};
    CHECK(ab_font_wrap_codes(&font, hyphen, 3, 22, &result, NULL) &&
          result.count == 2 && lines[0].count == 2 && lines[0].width == 21 &&
          lines[1].start == 2);
    CHECK(ab_font_wrap_codes(&font, blank, 2, 16, &result, NULL) && result.count == 2 &&
          result.widest_line == 0 && lines[0].width == -1 && lines[1].count == 0);
    CHECK(ab_font_wrap_codes(NULL, NULL, SIZE_MAX, -INFINITY, &result, NULL) &&
          result.count == 1 && !lines[0].count && !result.widest_line);
    CHECK(ab_font_wrap_codes(NULL, NULL, 0, INFINITY, &result, NULL) && !result.count);
    result.count = 7;
    result.widest_line = 19;
    CHECK(!ab_font_wrap_codes(&font, pair, 2, 10, &result, NULL) && result.count == 7 &&
          result.widest_line == 19);
    result.capacity = 1;
    CHECK(!ab_font_wrap_codes(&font, pair, 2, 22, &result, NULL) && result.count == 7 &&
          result.widest_line == 19);
    CHECK(!ab_font_wrap_codes(&font, pair, 2, NAN, &result, NULL));
    CHECK(!ab_font_wrap_codes(NULL, pair, 2, 20, &result, NULL));
    CHECK(!ab_font_wrap_codes(&font, NULL, 1, 20, &result, NULL));
    CHECK(!ab_font_wrap_codes(&font, pair, 2, 20, NULL, NULL));
    puts("Font wrap strict width, word/character splits, whitespace/hard breaks, "
         "empty branches, progress/capacity bounds and failure-output checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static int hex_digit(int character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    return character >= 'a' && character <= 'f' ? character - 'a' + 10 : -1;
}

static bool compare_line(FILE *file, const uint32_t *codes, AbFontWrappedLine line,
                         size_t test, size_t index, size_t *bytes) {
    uint32_t size;
    uint8_t buffer[512];
    CHECK(words(file, &size, 1) && size <= sizeof(buffer));
    for (size_t position = 0; position < size; ++position) {
        int high = hex_digit(fgetc(file)), low = hex_digit(fgetc(file));
        CHECK(high >= 0 && low >= 0);
        buffer[position] = (uint8_t)(high * 16 + low);
    }
    CHECK(fgetc(file) == '\n');
    uint8_t encoded[512];
    size_t encoded_length;
    CHECK(ab_utf8_from_codes(codes + line.start, line.count, encoded, sizeof(encoded),
                             &encoded_length) &&
          encoded_length == size && (!size || memcmp(buffer, encoded, size) == 0));
    uint32_t native[512];
    size_t length;
    CHECK(ab_utf8_to_codes((AbSpan){buffer, size}, native, 512, &length));
    if (length != line.count ||
        (length && memcmp(native, codes + line.start, length * sizeof(*native)) != 0)) {
        fprintf(stderr, "Font wrap case %zu line %zu: C %zu codes, native %zu\n", test,
                index, line.count, length);
        return false;
    }
    *bytes += size;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_WRAP_V1 4096\n") == 0);
    const uint32_t ids[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};
    AbFontGlyph glyphs[7];
    for (size_t index = 0; index < 7; ++index) {
        glyphs[index] = (AbFontGlyph){.code = ids[index],
                                      .width = (int16_t)(8 + index),
                                      .height = (int16_t)(12 + index)};
    }
    AbBitmapFont font = {.glyphs = glyphs,
                         .count = 7,
                         .tracking = 1,
                         .leading = 30,
                         .max_ascending = 20,
                         .max_descending = 5};
    size_t total_lines = 0, empty_lines = 0, total_bytes = 0;
    size_t case_count = sizeof(ui_text_wrap_cases) / sizeof(*ui_text_wrap_cases);
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t input[2], expected[2];
        CHECK(words(file, input, 2) && input[0] < case_count &&
              words(file, expected, 2));
        const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
        uint32_t codes[128];
        size_t length;
        CHECK(ab_utf8_to_codes((AbSpan){(const uint8_t *)source->bytes, source->size},
                               codes, 128, &length));
        AbFontWrappedLine lines[128];
        AbFontWrap wrapped = {.lines = lines, .capacity = 128};
        CHECK(
            ab_font_wrap_codes(&font, codes, length, number(input[1]), &wrapped, NULL));
        if (wrapped.count != expected[0] ||
            raw((float)wrapped.widest_line) != expected[1]) {
            fprintf(stderr,
                    "Font wrap case %zu source %" PRIu32 ": C %zu lines/%d "
                    "width, native %" PRIu32 " lines/%g width\n",
                    test, input[0], wrapped.count, wrapped.widest_line, expected[0],
                    (double)number(expected[1]));
            return false;
        }
        for (size_t index = 0; index < wrapped.count; ++index) {
            CHECK(compare_line(file, codes, lines[index], test, index, &total_bytes));
            empty_lines += lines[index].count == 0;
        }
        total_lines += wrapped.count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text wrap reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native wrapping cases match across %zu input strings: "
           "%zu lines, %zu empty lines, all %zu native UTF8 bytes and code slices, "
           "all widest-line bits.\n",
           case_count, total_lines, empty_lines, total_bytes);
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    fclose(file);
    return okay ? 0 : 1;
}
