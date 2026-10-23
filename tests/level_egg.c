#include "ab/data_order.h"
#include "ab/level_egg.h"
#include "level_egg_cases.h"

#include <inttypes.h>
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

typedef struct {
    LevelEggCase input;
    uint32_t queries, achievements;
} Host;

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static bool same(AbSpan value, const char *expected) {
    return value.size == strlen(expected) &&
           memcmp(value.data, expected, value.size) == 0;
}

static bool unlocked(void *context, AbSpan egg, AbDataValue *output, AbError *error) {
    Host *host = context;
    (void)error;
    CHECK(same(egg, level_egg_names[host->input.script][2]));
    ++host->queries;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = host->input.unlocked};
    return true;
}

static bool achieve(void *context, AbSpan egg, AbError *error) {
    Host *host = context;
    (void)error;
    CHECK(same(egg, level_egg_names[host->input.script][2]));
    ++host->achievements;
    return true;
}

static bool compare(FILE *file, const size_t *indices, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
        CHECK(indices[index] == expected);
    }
    return true;
}

static bool cases(FILE *file) {
    AbDataEntry world_entries[AB_LEVEL_EGG_MAXIMUM];
    AbDataEntry joint_entries[AB_LEVEL_EGG_MAXIMUM];
    AbDataEntry endpoint_fields[AB_LEVEL_EGG_MAXIMUM][2];
    AbDataTable joint_tables[AB_LEVEL_EGG_MAXIMUM];
    char names[AB_LEVEL_EGG_MAXIMUM][12];
    size_t world_indices[AB_LEVEL_EGG_MAXIMUM], joint_indices[AB_LEVEL_EGG_MAXIMUM];
    AbStringOrderNode world_nodes[AB_LEVEL_EGG_MAXIMUM];
    AbStringOrderNode joint_nodes[AB_LEVEL_EGG_MAXIMUM];
    AbStringOrderWorkspace world_order = {world_indices, AB_LEVEL_EGG_MAXIMUM,
                                          world_nodes, AB_LEVEL_EGG_MAXIMUM};
    AbStringOrderWorkspace joint_order = {joint_indices, AB_LEVEL_EGG_MAXIMUM,
                                          joint_nodes, AB_LEVEL_EGG_MAXIMUM};
    AbDataTable empty = {0};
    char header[80];
    if (file) {
        CHECK(fgets(header, sizeof(header), file));
        CHECK(strcmp(header, "AB_ANDROID_LEVEL_EGG_V1 1152\n") == 0);
    }
    for (uint32_t sample = 0; sample < AB_LEVEL_EGG_CASES; ++sample) {
        Host host = {.input = level_egg_case(sample)};
        LevelEggCase input = host.input;
        const char *const *source = level_egg_names[input.script];
        const AbLevelEggScript *script = ab_level_egg_script(span(source[0]));
        CHECK(script && strcmp(script->object, source[1]) == 0 &&
              strcmp(script->egg, source[2]) == 0);
        for (uint32_t index = 0; index < AB_LEVEL_EGG_MAXIMUM; ++index) {
            level_egg_key(index, names[index]);
            world_entries[index] =
                (AbDataEntry){text(index == input.watched ? source[1] : names[index]),
                              {.kind = AB_DATA_TABLE, .table = &empty}};
            for (uint32_t endpoint = 0; endpoint < 2; ++endpoint)
                endpoint_fields[index][endpoint] = (AbDataEntry){
                    text(endpoint ? "end2" : "end1"),
                    text((index + endpoint) % 3 == 0 ? source[1] : "other")};
            joint_tables[index] = (AbDataTable){endpoint_fields[index], 2, 2, NULL};
            joint_entries[index] =
                (AbDataEntry){text(names[index]),
                              {.kind = AB_DATA_TABLE, .table = joint_tables + index}};
        }
        AbDataTable world = {world_entries, input.world, AB_LEVEL_EGG_MAXIMUM, NULL};
        AbDataTable joints = {joint_entries, input.joints, AB_LEVEL_EGG_MAXIMUM, NULL};
        AbDataEntry fields[] = {
            {text("world"), {.kind = AB_DATA_TABLE, .table = &world}},
            {text("joints"), {.kind = AB_DATA_TABLE, .table = &joints}}};
        AbDataTable globals = {fields, 2, 2, NULL};
        AbLuaData data = {.globals = &globals};
        AbDataValue eagle = {0};
        if (input.eagle == 1)
            eagle.kind = AB_DATA_BOOLEAN;
        else if (input.eagle == 2)
            eagle.kind = AB_DATA_NUMBER;
        else if (input.eagle == 3)
            eagle = text("");
        AbDataEntry physical_name = {text("name"),
                                     text(input.match ? source[1] : "other")};
        AbDataTable physical = {&physical_name, 1, 1, NULL};
        AbLevelEggHost callbacks = {.context = &host,
                                    .full_version = input.full,
                                    .eagle_bait_launched = &eagle,
                                    .unlocked = unlocked,
                                    .achieve = achieve};
        AbError error = {0};
        size_t world_count = SIZE_MAX, joint_count = SIZE_MAX;
        CHECK(ab_level_egg_prepare(script, &data, callbacks, &error));
        CHECK(ab_level_egg_removed(script, &physical, callbacks, &error));
        CHECK(ab_data_string_order_pruned(&world, level_egg_capacity(input.world),
                                          &world_order, &world_count, &error));
        CHECK(ab_data_string_order_pruned(&joints, level_egg_capacity(input.joints),
                                          &joint_order, &joint_count, &error));
        CHECK(world_count <= input.world && joint_count <= input.joints);
        if (file) {
            uint32_t expected[4];
            CHECK(fscanf(file, "%8" SCNx32 "%8" SCNx32 "%8" SCNx32 "%8" SCNx32,
                         expected, expected + 1, expected + 2, expected + 3) == 4);
            if (expected[0] != host.queries || expected[1] != host.achievements ||
                expected[2] != world_count || expected[3] != joint_count) {
                fprintf(stderr, "Native golden-egg case %u diverged\n", sample);
                return false;
            }
            CHECK(compare(file, world_indices, world_count));
            CHECK(compare(file, joint_indices, joint_count));
        }
    }
    if (file) {
        CHECK(fscanf(file, " ") == 0);
        CHECK(fgets(header, sizeof(header), file));
        CHECK(strcmp(header, "AB level-egg reference complete\n") == 0);
        CHECK(fgetc(file) == EOF && !ferror(file));
        puts(
            "All 1152 native egg hooks, unlock queries and pruned table orders match.");
    }
    return true;
}

static bool edges(void) {
    AbError error = {0};
    AbLuaData data = {0};
    CHECK(!ab_level_egg_script(span("LevelP3_271")));
    CHECK(!ab_level_egg_script((AbSpan){NULL, 12}));
    CHECK(!ab_level_remove_loaded_object(NULL, span("missing"), &error));
    CHECK(!ab_level_remove_loaded_object(&data, span("missing"), &error));
    CHECK(!ab_level_egg_prepare(NULL, &data, (AbLevelEggHost){0}, &error));
    CHECK(!ab_level_egg_removed(NULL, NULL, (AbLevelEggHost){0}, &error));
    AbDataTable table = {0};
    AbStringOrderWorkspace order = {0};
    size_t count = 99;
    CHECK(ab_data_string_order_pruned(&table, 0, &order, &count, &error));
    CHECK(count == 0);
    CHECK(!ab_data_string_order_pruned(&table, 0, &order, NULL, &error));
    AbDataEntry entry = {.key = {.kind = AB_DATA_STRING}};
    size_t index = 41;
    AbStringOrderNode node;
    table = (AbDataTable){&entry, 1, 1, NULL};
    order = (AbStringOrderWorkspace){&index, 1, &node, 1};
    CHECK(ab_data_string_order_pruned(&table, 1, &order, &count, &error));
    CHECK(count == 0 && index == 41);
    CHECK(!ab_data_string_order(&table, 1, &order, &error));
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
