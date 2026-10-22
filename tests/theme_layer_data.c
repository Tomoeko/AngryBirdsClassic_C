#include "ab/theme_layer_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme data check failed at %d\n", __LINE__);              \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static AbDataValue key(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static bool edges(void) {
    AbDataEntry entries[] = {
        {.key = key(2),
         .value = {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"sprite", 6}}},
        {.key = key(5), .value = {.kind = AB_DATA_NUMBER}},
        {.key = key(6), .value = {.kind = AB_DATA_NUMBER, .bits = bits(-15)}}};
    AbDataTable table = {.entries = entries, .count = 3, .capacity = 3};
    AbSpriteRegion sprite = {.width = 100, .height = 50, .pivot_x = -2, .pivot_y = 3};
    AbThemeLayerDefinition definition;
    AbError error = {0};
    CHECK(ab_theme_layer_definition(&table, &sprite, &definition, &error) &&
          definition.has_base_sprite && definition.base.width == 100 &&
          definition.base.pivot_x == -1 && definition.base.repeats &&
          definition.base.x_offset == -15 && definition.base.phase == -15 &&
          definition.base.scale == 1 && definition.parallax == 1);
    CHECK(ab_theme_layer_definition(&table, NULL, &definition, &error) &&
          !definition.has_base_sprite && definition.base.width == 0);
    uint8_t retained[sizeof(definition)];
    memcpy(retained, &definition, sizeof(definition));
    entries[2].value =
        (AbDataValue){.kind = AB_DATA_STRING, .string = {(const uint8_t *)"1", 1}};
    CHECK(!ab_theme_layer_definition(&table, &sprite, &definition, &error) &&
          memcmp(&definition, retained, sizeof(definition)) == 0);
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
    CHECK(fgets(header, sizeof(header), file));
    bool composite = strcmp(header, "AB_ANDROID_THEME_LAYER_CONSTRUCT_V2 4096\n") == 0;
    CHECK(composite ||
          strcmp(header, "AB_ANDROID_THEME_LAYER_CONSTRUCT_V1 4096\n") == 0);
    size_t present = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17], expected[19], actual[19];
        CHECK(words(file, input, 17) && words(file, expected, 19) && input[0] <= 1);
        AbDataTable object = {0};
        AbDataEntry entries[7] = {
            {.key = key(2),
             .value = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)"sprite", 6}}}};
        for (size_t index = 0; index < 6; ++index) {
            uint32_t kind = input[5 + index * 2];
            CHECK(kind < 5);
            entries[index + 1].key =
                index == 5 ? (AbDataValue){.kind = AB_DATA_STRING,
                                           .string = {(const uint8_t *)"v", 1}}
                           : key((float)index + 3);
            AbDataValue value = {0};
            if (kind == 1 || kind == 2) {
                value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
            } else if (kind == 3) {
                value =
                    (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[6 + index * 2]};
            } else if (kind == 4) {
                value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &object};
            }
            entries[index + 1].value = value;
        }
        AbDataTable table = {.entries = entries, .count = 7, .capacity = 7};
        AbSpriteRegion region = {.width = (int16_t)input[1],
                                 .height = (int16_t)input[2],
                                 .pivot_x = (int16_t)(int32_t)input[3],
                                 .pivot_y = (int16_t)(int32_t)input[4]};
        AbThemeLayerDefinition definition;
        AbError error = {0};
        if (composite) {
            AbThemeSpriteMetrics metrics = {input[0] != 0, (int32_t)input[1],
                                            (int32_t)input[2], (int32_t)input[3],
                                            (int32_t)input[4]};
            CHECK(ab_theme_layer_definition_metrics(&table, &metrics, &definition,
                                                    &error));
        } else {
            CHECK(ab_theme_layer_definition(&table, input[0] ? &region : NULL,
                                            &definition, &error));
        }
        const AbThemeLayerBase *base = &definition.base;
        actual[0] = definition.has_base_sprite;
        actual[1] = bits(base->width);
        actual[2] = bits(definition.height);
        actual[3] = bits(base->pivot_x);
        actual[4] = bits(definition.pivot_y);
        actual[5] = bits(definition.parallax);
        actual[6] = bits(base->scale);
        actual[7] = base->repeats;
        actual[8] = bits(base->x_offset);
        actual[9] = bits(base->speed);
        for (size_t channel = 0; channel < 4; ++channel) {
            actual[10 + channel] = bits(base->color[channel]);
        }
        actual[14] = bits(base->phase);
        actual[15] = bits(base->vertical_offset);
        actual[16] = actual[17] = actual[18] = 0;
        for (size_t index = 0; index < 19; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Theme constructor %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, index, actual[index], expected[index]);
                return false;
            }
        }
        present += definition.has_base_sprite;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme layer constructor reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native theme constructors match: %zu present/%zu missing "
           "%s sprites.\n",
           present, 4096 - present, composite ? "composite" : "ordinary");
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
