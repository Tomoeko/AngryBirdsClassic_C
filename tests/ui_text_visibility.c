#include "ab/ui_text_draw.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI text visibility failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static int32_t signed_word(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value
                              : (int32_t)((int64_t)value - INT64_C(4294967296));
}

static bool edge_checks(void) {
    CHECK(!ab_ui_text_on_screen(20, 5, 320, 1, AB_ANCHOR_TOP, 320));
    CHECK(ab_ui_text_on_screen(20, 5, -25, 1, AB_ANCHOR_TOP, 320));
    CHECK(!ab_ui_text_on_screen(20, 5, -26, 1, AB_ANCHOR_TOP, 320));
    CHECK(ab_ui_text_on_screen(20, 5, 0, 1, AB_ANCHOR_VCENTER, 320));
    CHECK(ab_ui_text_on_screen(20, 5, 0, 1, AB_ANCHOR_BOTTOM, 320));
    CHECK(!ab_ui_text_on_screen(20, 5, 20, -1, AB_ANCHOR_TOP, 320));
    CHECK(ab_ui_text_on_screen(20, 5, NAN, 1, AB_ANCHOR_TOP, 320));
    CHECK(ab_ui_text_on_screen(20, 5, 0, NAN, AB_ANCHOR_TOP, 320));
    CHECK(!ab_ui_text_on_screen(20, 5, INFINITY, 1, AB_ANCHOR_TOP, 320));
    CHECK(!ab_ui_text_on_screen(20, 5, -INFINITY, 1, AB_ANCHOR_TOP, 320));
    CHECK(ab_ui_text_on_screen(20, 5, 0, 1, INT32_MIN, 320));
    puts("UI text strict/inclusive edges, negative-scale interval, anchor fallback "
         "and IEEE unordered visibility checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_TEXT_VISIBILITY_V1 8192\n") == 0);
    size_t accepted = 0, nonfinite = 0;
    for (size_t test = 0; test < 8192; ++test) {
        uint32_t input[9], expected = 0;
        CHECK(words(file, input, 9) && words(file, &expected, 1) && expected <= 1);
        float y = number(input[3]), sy = number(input[5]);
        bool actual =
            ab_ui_text_on_screen(signed_word(input[0]), signed_word(input[1]), y, sy,
                                 signed_word(input[6]), signed_word(input[8]));
        if (actual != (expected != 0)) {
            fprintf(stderr, "UI text visibility differs at case %zu\n", test);
            return false;
        }
        accepted += actual;
        nonfinite += !isfinite(y) || !isfinite(sy);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI text visibility reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 native text visibility decisions match: %zu accepted, "
           "%zu active nonfinite-input cases; x/width/horizontal fields ignored.\n",
           accepted, nonfinite);
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
