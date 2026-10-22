#include "ab/theme_colors.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme color check failed at %d\n", __LINE__);             \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

typedef struct {
    AbDataEntry groups[3], values[3][3];
    AbDataTable root, tables[3];
} Data;

static void data_init(Data *data, const uint32_t input[10]) {
    const char *const groups[] = {"color", "groundColor", "backgroundTintColor"};
    const char *const channels[] = {"r", "g", "b"};
    data->root = (AbDataTable){.entries = data->groups, .count = 3, .capacity = 3};
    for (size_t group = 0; group < 3; ++group) {
        data->tables[group] =
            (AbDataTable){.entries = data->values[group], .count = 3, .capacity = 3};
        data->groups[group] = (AbDataEntry){
            .key = text(groups[group]),
            .value = {.kind = AB_DATA_TABLE, .table = data->tables + group}};
        for (size_t channel = 0; channel < 3; ++channel) {
            data->values[group][channel] =
                (AbDataEntry){.key = text(channels[channel]),
                              .value = {.kind = AB_DATA_NUMBER,
                                        .bits = input[1 + group * 3 + channel]}};
        }
    }
    AbDataValue *tint = &data->groups[2].value;
    if (input[0] == 0) {
        *tint = (AbDataValue){0};
    } else if (input[0] < 3) {
        *tint = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = input[0] == 2};
    } else if (input[0] == 3) {
        *tint = (AbDataValue){.kind = AB_DATA_NUMBER};
    } else if (input[0] == 4) {
        *tint = text("unused");
    }
}

static bool edges(void) {
    uint32_t input[] = {0,         UINT32_C(0x80000000),
                        bits(146), bits(204),
                        bits(255), bits(0),
                        bits(0),   0,
                        0,         0};
    Data data;
    data_init(&data, input);
    AbThemeColors colors;
    AbError error = {0};
    CHECK(ab_theme_colors(&data.root, &colors, &error) &&
          bits(colors.background[0]) == UINT32_C(0x80000000) &&
          colors.background[1] == 146 && colors.ground[0] == 1 &&
          colors.tint[0] == 255);
    uint8_t retained[sizeof(colors)];
    memcpy(retained, &colors, sizeof(colors));
    data.values[0][0].value = text("1");
    CHECK(!ab_theme_colors(&data.root, &colors, &error) &&
          memcmp(retained, &colors, sizeof(colors)) == 0);
    data.values[0][0].value =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x7fc00000)};
    CHECK(!ab_theme_colors(&data.root, &colors, &error) &&
          memcmp(retained, &colors, sizeof(colors)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_THEME_COLORS_V1 8192\n") == 0);
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[10], expected[9], actual[9];
        CHECK(words(file, input, 10) && words(file, expected, 9) && input[0] < 6);
        Data data;
        data_init(&data, input);
        AbThemeColors colors;
        AbError error = {0};
        CHECK(ab_theme_colors(&data.root, &colors, &error));
        for (size_t index = 0; index < 3; ++index) {
            actual[index] = bits(colors.background[index]);
            actual[3 + index] = bits(colors.ground[index]);
            actual[6 + index] = bits(colors.tint[index]);
        }
        for (size_t index = 0; index < 9; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Theme color %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, index, actual[index], expected[index]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme colors reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 native theme color constructions match, including signed zeros, "
         "ground reciprocal multiplication and all six tint cases.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
