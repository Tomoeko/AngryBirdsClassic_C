#include "ab/game_behavior.h"
#include "game_behavior_add_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Behavior construction check at %d: %s\n", __LINE__,       \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[16], output[52];
    AbStringMap *maps[2];
    AbGameBehavior instances[100];
    AbGameBehavior *items[6][4];
    AbGameBehaviorGroup groups[6];
    AbGameBehaviorRegistration registration;
    AbDataTable receiver, properties;
} BehaviorAddFixture;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t group_id(const BehaviorAddFixture *fixture, uintptr_t value) {
    for (size_t index = 0; value && index < 6; ++index)
        if (value == (uintptr_t)(fixture->groups + index))
            return (uint32_t)index + 1u;
    return 0;
}

static uint32_t instance_id(const BehaviorAddFixture *fixture,
                            const AbGameBehavior *instance) {
    for (size_t index = 0; instance && index < 100; ++index)
        if (instance == fixture->instances + index)
            return (uint32_t)index + 1u;
    return 0;
}

static AbDataValue properties(BehaviorAddFixture *fixture) {
    switch (fixture->input[7]) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN,
                                 .bits = fixture->input[7] - 1u};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[8]};
        case 4:
            return (AbDataValue){.kind = AB_DATA_STRING, .string = text("payload")};
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &fixture->properties};
    }
}

static bool create(void *context, AbSpan name, AbDataValue receiver, AbDataValue value,
                   AbGameBehavior **output, AbError *error) {
    (void)error;
    BehaviorAddFixture *fixture = context;
    if (!fixture->input[9])
        return false;
    CHECK(receiver.kind == AB_DATA_TABLE && receiver.table == &fixture->receiver);
    CHECK(name.size == strlen(behavior_add_names[fixture->input[5]]) &&
          !memcmp(name.data, behavior_add_names[fixture->input[5]], name.size));
    uintptr_t handle = 0;
    bool present = false;
    CHECK(ab_string_map_get(fixture->registration.groups, name, &handle, &present,
                            NULL) &&
          present && group_id(fixture, handle));
    AbGameBehaviorGroup *cached = (AbGameBehaviorGroup *)handle;
    fixture->output[1]++;
    fixture->output[2] = fixture->input[5] + 1u;
    fixture->output[3] = 101;
    const uint32_t tags[] = {0, 1, 3, 4, 5};
    fixture->output[4] = tags[value.kind];
    fixture->output[5] = value.kind == AB_DATA_TABLE    ? 103u
                         : value.kind == AB_DATA_STRING ? 1u
                                                        : value.bits;
    fixture->output[48] = group_id(fixture, handle);
    if (fixture->input[10] & 1u)
        CHECK(ab_string_map_set(fixture->registration.groups, name,
                                (uintptr_t)(fixture->groups + 5), true, NULL));
    if (fixture->input[10] & 2u)
        fixture->registration.groups = fixture->maps[1];
    if (fixture->input[10] & 4u) {
        size_t index = 0;
        while (index < cached->capacity && cached->items[index])
            ++index;
        CHECK(index < cached->capacity);
        cached->items[index] = fixture->instances + 89;
    }
    *output = fixture->instances + fixture->input[11] - 1u;
    return true;
}

static bool prepare(BehaviorAddFixture *fixture, uint32_t sample) {
    behavior_add_case(sample, fixture->input);
    for (size_t index = 0; index < 6; ++index) {
        fixture->groups[index] = (AbGameBehaviorGroup){fixture->items[index], 4};
        size_t count = index == 4 ? 0 : index == 5 ? 1 : fixture->input[12];
        for (size_t slot = 0; slot < count; ++slot)
            fixture->items[index][slot] =
                fixture->instances + (index + 1u) * 10u + slot;
    }
    CHECK(ab_string_map_open(fixture->input[1], 8, 16384, fixture->maps, NULL) &&
          ab_string_map_open(0, 8, 16384, fixture->maps + 1, NULL));
    for (uint32_t index = 0; index < fixture->input[2]; ++index) {
        uint32_t name =
            (fixture->input[3] + (fixture->input[4] ? 4u - index : index)) % 4u;
        uint32_t kind = name == fixture->input[5] ? fixture->input[6] : 2u;
        CHECK(ab_string_map_set(fixture->maps[0], text(behavior_add_names[name]),
                                kind == 2 ? (uintptr_t)(fixture->groups + name) : 0,
                                kind != 0, NULL));
    }
    for (uint32_t index = 0; index < 4; ++index)
        CHECK(ab_string_map_set(fixture->maps[1], text(behavior_add_names[index]),
                                (uintptr_t)(fixture->groups + 5), true, NULL));
    fixture->registration = (AbGameBehaviorRegistration){
        .groups = fixture->maps[0],
        .receiver = {.kind = AB_DATA_TABLE, .table = &fixture->receiver},
        .new_groups = fixture->groups + 4,
        .group_capacity = 1,
        .context = fixture,
        .create = create};
    return true;
}

static bool snapshot(BehaviorAddFixture *fixture) {
    uint32_t *output = fixture->output;
    output[6] = fixture->registration.groups == fixture->maps[0] ? 1u : 2u;
    output[7] = (uint32_t)fixture->registration.group_count;
    for (size_t map = 0; map < 2; ++map) {
        for (size_t name = 0; name < 4; ++name) {
            uintptr_t value = 0;
            bool present = false;
            CHECK(ab_string_map_get(fixture->maps[map], text(behavior_add_names[name]),
                                    &value, &present, NULL));
            output[8 + map * 4 + name] = present ? group_id(fixture, value) : 0;
        }
        size_t cursor = 0, index = 0;
        for (;;) {
            AbStringMapEntry entry;
            CHECK(ab_string_map_next(fixture->maps[map], &cursor, &entry, NULL));
            if (!entry.present)
                break;
            CHECK(index < 4);
            for (size_t name = 0; name < 4; ++name)
                if (entry.key.size == strlen(behavior_add_names[name]) &&
                    !memcmp(entry.key.data, behavior_add_names[name], entry.key.size))
                    output[40 + map * 4 + index] = (uint32_t)name + 1u;
            ++index;
        }
    }
    for (size_t group = 0; group < 6; ++group)
        for (size_t slot = 0; slot < 4; ++slot)
            output[16 + group * 4 + slot] =
                instance_id(fixture, fixture->items[group][slot]);
    if (output[48]) {
        const AbGameBehaviorGroup *group = fixture->groups + output[48] - 1u;
        for (size_t index = 0; index < group->capacity && group->items[index]; ++index)
            ++output[49];
    }
    return true;
}

static bool construction_limits(void) {
    BehaviorAddFixture fixture = {0};
    bool okay = prepare(&fixture, 0);
    if (okay) {
        fixture.input[5] = 0;
        fixture.input[9] = 1;
        fixture.input[10] = 0;
        AbSpan name = text(behavior_add_names[0]);
        okay = ab_string_map_reset(fixture.maps[0], 0, NULL);
        fixture.registration.create = NULL;
        okay = okay &&
               !ab_game_object_add_behavior(&fixture.registration, name,
                                            (AbDataValue){0}, NULL) &&
               fixture.registration.group_count == 1 && !fixture.output[1];
        fixture.registration.create = create;
        fixture.groups[4].capacity = 0;
        okay = okay &&
               !ab_game_object_add_behavior(&fixture.registration, name,
                                            (AbDataValue){0}, NULL) &&
               fixture.registration.group_count == 1 && fixture.output[1] == 1;
        fixture.groups[4].capacity = 1;
        okay = okay &&
               ab_game_object_add_behavior(&fixture.registration, name,
                                           (AbDataValue){0}, NULL) &&
               fixture.output[1] == 2 && fixture.items[4][0] == fixture.instances + 90;
        okay = okay &&
               !ab_game_object_add_behavior(&fixture.registration, name,
                                            (AbDataValue){0}, NULL) &&
               fixture.output[1] == 3 && fixture.items[4][0] == fixture.instances + 90;
        fixture.registration.groups = fixture.maps[1];
        fixture.registration.new_groups = NULL;
        fixture.registration.group_count = SIZE_MAX;
        fixture.registration.group_capacity = 0;
        okay = okay &&
               ab_game_object_add_behavior(&fixture.registration, name,
                                           (AbDataValue){0}, NULL) &&
               fixture.output[1] == 4 && fixture.items[5][1] == fixture.instances + 90;
        okay = okay && ab_string_map_reset(fixture.maps[1], 0, NULL);
        fixture.registration.group_count = 0;
        okay = okay &&
               !ab_game_object_add_behavior(&fixture.registration, name,
                                            (AbDataValue){0}, NULL) &&
               fixture.output[1] == 4 && !fixture.registration.group_count;
        fixture.registration.new_groups = fixture.groups + 4;
        fixture.registration.group_capacity = 1;
        okay = okay &&
               !ab_game_object_add_behavior(&fixture.registration, name,
                                            (AbDataValue){0}, NULL) &&
               fixture.output[1] == 4 && !fixture.registration.group_count;
    }
    for (size_t index = 0; index < 2; ++index)
        ab_string_map_close(fixture.maps[index]);
    CHECK(okay);
    return true;
}

static bool read_words(FILE *file, const uint32_t *actual, size_t count,
                       uint32_t sample, const char *phase) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Behavior construction %u %s word %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, phase, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_GAME_BEHAVIOR_ADD_V1 2048\n"));
    size_t failures = 0;
    for (uint32_t sample = 0; sample < BEHAVIOR_ADD_CASES; ++sample) {
        BehaviorAddFixture fixture = {0};
        bool okay = prepare(&fixture, sample);
        if (okay) {
            fixture.output[0] = !ab_game_object_add_behavior(
                &fixture.registration, text(behavior_add_names[fixture.input[5]]),
                properties(&fixture), NULL);
            failures += fixture.output[0] != 0;
            okay = snapshot(&fixture) &&
                   read_words(file, fixture.input, 16, sample, "input") &&
                   read_words(file, fixture.output, 52, sample, "state");
        }
        for (size_t index = 0; index < 2; ++index)
            ab_string_map_close(fixture.maps[index]);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB game behavior add reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %u behavior construction cases, %zu factory failures\n",
           BEHAVIOR_ADD_CASES, failures);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 1 && argc != 2)
        return 2;
    if (!construction_limits())
        return 1;
    if (argc == 1)
        return 0;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = reference(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
