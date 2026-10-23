#include "ab/level_clock.h"
#include "level_clock_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool cases(FILE *file) {
    char header[80];
    if (file) {
        CHECK(fgets(header, sizeof(header), file));
        CHECK(strcmp(header, "AB_ANDROID_LEVEL_CLOCK_V1 4096\n") == 0);
    }
    for (uint32_t sample = 0; sample < AB_LEVEL_CLOCK_CASES; ++sample) {
        LevelClockCase input = level_clock_case(sample);
        AbDataEntry positions[4][2], entries[4];
        AbDataTable objects[4];
        for (size_t index = 0; index < 4; ++index) {
            positions[index][0] =
                (AbDataEntry){text("x"), number(input.positions[index * 2])};
            positions[index][1] =
                (AbDataEntry){text("y"), number(input.positions[index * 2 + 1])};
            objects[index] = (AbDataTable){positions[index], 2, 2, NULL};
            entries[index] =
                (AbDataEntry){text(level_clock_names[index]),
                              {.kind = AB_DATA_TABLE, .table = objects + index}};
            if (input.absent == index)
                entries[index].value =
                    (AbDataValue){.kind = sample & 1 ? AB_DATA_BOOLEAN : AB_DATA_NIL};
        }
        AbDataTable world = {entries, 4, 4, NULL};
        AbDataEntry world_field = {text("world"),
                                   {.kind = AB_DATA_TABLE, .table = &world}};
        AbDataTable globals = {&world_field, 1, 1, NULL};
        AbLuaData data = {.globals = &globals};
        AbCalendarTime time = {.minutes = input.minutes, .seconds = input.seconds};
        AbError error = {0};
        CHECK(ab_level_clock_prepare(&data, &time, &error));
        if (file) {
            uint32_t queries;
            CHECK(fscanf(file, "%8" SCNx32, &queries) == 1 && queries == 1);
            for (size_t index = 0; index < 8; ++index) {
                uint32_t expected;
                uint32_t actual = positions[index / 2][index % 2].value.bits;
                CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
                if (actual != expected) {
                    fprintf(stderr,
                            "Native clock case %u field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            sample, index, actual, expected);
                    return false;
                }
            }
            CHECK(fgetc(file) == '\n');
        }
    }
    if (file) {
        CHECK(fgets(header, sizeof(header), file));
        CHECK(strcmp(header, "AB level-clock reference complete\n") == 0);
        CHECK(fgetc(file) == EOF && !ferror(file));
        puts("All 4096 native clock preparations match every output bit.");
    }
    return true;
}

static bool edges(void) {
    AbError error = {0};
    AbCalendarTime time = {0};
    AbLuaData data = {0};
    CHECK(!ab_level_clock_prepare(NULL, &time, &error));
    CHECK(!ab_level_clock_prepare(&data, NULL, &error));
    AbDataTable world = {0};
    AbDataEntry entry = {text("world"), {.kind = AB_DATA_TABLE, .table = &world}};
    AbDataTable globals = {&entry, 1, 1, NULL};
    data.globals = &globals;
    CHECK(ab_level_clock_prepare(&data, &time, &error));
    time.minutes = INFINITY;
    CHECK(!ab_level_clock_prepare(&data, &time, &error));
    time.minutes = 60;
    CHECK(!ab_level_clock_prepare(&data, &time, &error));
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return EXIT_FAILURE;
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !file)
        return EXIT_FAILURE;
    bool okay = edges() && cases(file);
    if (file && fclose(file) != 0)
        okay = false;
    return okay ? EXIT_SUCCESS : EXIT_FAILURE;
}
