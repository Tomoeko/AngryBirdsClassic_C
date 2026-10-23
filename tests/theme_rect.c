#include "ab/theme_rect.h"
#include "theme_rect_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Rectangle theme check failed at %d\n", __LINE__);         \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static uint32_t bits(float value) {
    uint32_t word = 0;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static AbDataValue number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static bool edges(void) {
    AbDataTable empty = {0};
    AbDataEntry entry = {text("rect"), {.kind = AB_DATA_TABLE, .table = &empty}};
    AbDataTable layer = {&entry, 1, 1, NULL};
    AbThemeRect rect = {0};
    CHECK(!ab_theme_rect_read(NULL, &rect, NULL));
    CHECK(ab_theme_rect_read(&layer, &rect, NULL) && rect.color[3] == 0 &&
          rect.left == 0 && rect.bottom == 0 && rect.width == 1 && rect.height == 1);
    AbGameRectanglePlan plan = {.left = 13};
    bool active = true;
    CHECK(ab_theme_rect_plan(&rect, 800, 600, &plan, &active, NULL) && !active &&
          plan.left == 13);
    AbDataEntry fields[] = {
        {text("r"), number(300)},          {text("b"), number(127)},
        {text("a"), number(255)},          {text("relLeft"), number(-0.125f)},
        {text("relBottom"), number(0.5f)}, {text("relWidth"), number(0.75f)},
        {text("relHeight"), number(0.25f)}};
    AbDataTable values = {fields, 7, 7, NULL};
    entry.value.table = &values;
    CHECK(ab_theme_rect_read(&layer, &rect, NULL) &&
          ab_theme_rect_plan(&rect, 800, 600, &plan, &active, NULL) && active &&
          plan.argb == UINT32_C(0xff2c007f) && plan.left == -100 && plan.top == 300 &&
          plan.width == 600 && plan.height == 150 && plan.reset_state);
    uint8_t retained[sizeof(rect)];
    memcpy(retained, &rect, sizeof(retained));
    fields[6].value = text("1");
    CHECK(!ab_theme_rect_read(&layer, &rect, NULL) &&
          !memcmp(retained, &rect, sizeof(retained)));
    CHECK(!ab_theme_rect_plan(&rect, 0, 600, &plan, &active, NULL));
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_THEME_RECT_V1 4096\n"));
    const char *const names[] = {"r",       "g",         "b",        "a",
                                 "relLeft", "relBottom", "relWidth", "relHeight"};
    size_t draws = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[18], generated[18], expected[14], actual[14];
        CHECK(words(file, input, 18) && words(file, expected, 14));
        theme_rect_case(sample, generated);
        CHECK(!memcmp(input, generated, sizeof(input)));
        AbDataTable empty = {0};
        AbDataEntry fields[8];
        for (size_t field = 0; field < 8; ++field) {
            uint32_t kind = input[2 + field * 2];
            AbDataValue value = {0};
            if (kind == 1 || kind == 2)
                value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
            else if (kind == 3)
                value =
                    (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[3 + field * 2]};
            else if (kind == 4)
                value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
            fields[field] = (AbDataEntry){text(names[field]), value};
        }
        AbDataTable table = {fields, 8, 8, NULL};
        AbDataEntry entry = {text("rect"), {.kind = AB_DATA_TABLE, .table = &table}};
        AbDataTable layer = {&entry, 1, 1, NULL};
        AbThemeRect rect;
        AbGameRectanglePlan plan = {0};
        bool active = false;
        CHECK(ab_theme_rect_read(&layer, &rect, NULL) &&
              ab_theme_rect_plan(&rect, input[0], input[1], &plan, &active, NULL));
        for (size_t field = 0; field < 4; ++field)
            actual[field] = bits(rect.color[field]);
        actual[4] = bits(rect.left);
        actual[5] = bits(rect.bottom);
        actual[6] = bits(rect.width);
        actual[7] = bits(rect.height);
        actual[8] = active;
        actual[9] = plan.argb;
        actual[10] = (uint32_t)plan.left;
        actual[11] = (uint32_t)plan.top;
        actual[12] = (uint32_t)plan.width;
        actual[13] = (uint32_t)plan.height;
        for (size_t word = 0; word < 14; ++word) {
            if (actual[word] != expected[word]) {
                fprintf(stderr,
                        "Rectangle theme case %" PRIu32 " word %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        sample, word, actual[word], expected[word]);
                return false;
            }
        }
        draws += active;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB rectangle theme reference complete\n") &&
          fgetc(file) == EOF);
    printf("Native rectangle theme: 4096 constructors/draws, 57344 words, %zu "
           "active fills.\n",
           draws);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || !edges())
        return 1;
    if (argc == 1)
        return 0;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
