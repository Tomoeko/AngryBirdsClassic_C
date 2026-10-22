#include "ab/museum_bird_behavior.h"
#include "museum_bird_behavior_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Museum behavior check at %d: %s\n", __LINE__, #value);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[16], initial[8], events[4][8], count;
    AbDataTable tables[8], empty;
    AbDataEntry physical[2][1], scripts[2][1], other[2], launch[2][1];
    AbMuseumBirdBehavior behavior;
    AbMuseumBirdHost host;
    bool mutated, keep_disabled;
} MuseumFixture;

static const char *const strings[] = {"",
                                      "RedBird",
                                      "Other",
                                      "BOOMERANG",
                                      "MuseumBirdTrajectoryRenderer",
                                      "MuseumBirdCameraFollowBehavior"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue value(MuseumFixture *fixture, MuseumCaseValue input) {
    if (input.tag == 5)
        return (AbDataValue){.kind = AB_DATA_TABLE,
                             .table = fixture->tables + input.value - 1u};
    if (input.tag == 4)
        return text(strings[input.value]);
    return (AbDataValue){.kind = input.tag == 3   ? AB_DATA_NUMBER
                                 : input.tag == 1 ? AB_DATA_BOOLEAN
                                                  : AB_DATA_NIL,
                         .bits = input.value};
}

static MuseumCaseValue encode(const MuseumFixture *fixture, AbDataValue input) {
    if (input.kind == AB_DATA_TABLE) {
        for (uint32_t index = 0; index < 8; ++index)
            if (input.table == fixture->tables + index)
                return (MuseumCaseValue){5, index + 1u};
        return (MuseumCaseValue){5, UINT32_MAX};
    }
    if (input.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 6; ++index)
            if (input.string.size == strlen(strings[index]) &&
                !memcmp(input.string.data, strings[index], input.string.size))
                return (MuseumCaseValue){4, index};
        return (MuseumCaseValue){4, UINT32_MAX};
    }
    return (MuseumCaseValue){input.kind == AB_DATA_NUMBER    ? 3u
                             : input.kind == AB_DATA_BOOLEAN ? 1u
                                                             : 0u,
                             input.bits};
}

static bool set(MuseumFixture *fixture, const char *name, AbDataValue input) {
    AbDataTable *fields = &fixture->behavior.fields;
    for (size_t index = 0; index < fields->count; ++index) {
        AbDataValue key = fields->entries[index].key;
        if (key.kind == AB_DATA_STRING && key.string.size == strlen(name) &&
            !memcmp(key.string.data, name, key.string.size)) {
            fields->entries[index].value = input;
            return true;
        }
    }
    CHECK(fields->count < fields->capacity);
    fields->entries[fields->count++] = (AbDataEntry){text(name), input};
    return true;
}

static bool mutate(MuseumFixture *fixture) {
    if (fixture->mutated)
        return true;
    fixture->mutated = true;
    uint32_t changes = fixture->input[14];
    if (changes & 1u)
        CHECK(set(fixture, "physicsObject", value(fixture, (MuseumCaseValue){5, 2})));
    if (changes & 2u)
        fixture->host.flying_bird = value(fixture, (MuseumCaseValue){5, 2});
    if (changes & 4u)
        fixture->keep_disabled = true;
    if (changes & 8u)
        CHECK(set(fixture, "flying", value(fixture, (MuseumCaseValue){1, 0})));
    if (changes & 16u)
        CHECK(set(fixture, "gameObject", value(fixture, (MuseumCaseValue){5, 4})));
    if (changes & 32u)
        fixture->host.get_property = NULL;
    return true;
}

static bool service(void *context, const AbMuseumBirdCall *call, AbError *error) {
    (void)error;
    MuseumFixture *fixture = context;
    if (!(fixture->input[15] & (1u << call->operation)) ||
        (call->operation == AB_MUSEUM_TRAJECTORY_KEEP && fixture->keep_disabled))
        return false;
    CHECK(fixture->count < 4);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = (uint32_t)call->operation + 1u;
    MuseumCaseValue object = encode(fixture, call->object);
    event[1] = object.tag;
    event[2] = object.value;
    if (call->operation == AB_MUSEUM_CAMERA_SEQUENCE) {
        CHECK(call->sequence);
        event[5] = !strcmp(call->sequence, "FollowShotBirdCameraSequence") ? 1u
                   : !strcmp(call->sequence, "FollowBoomerangBird")        ? 2u
                                                                           : 0u;
        CHECK(event[5]);
    }
    event[7] = fixture->count;
    return mutate(fixture);
}

static bool property(void *context, AbDataValue object, AbSpan name,
                     AbDataValue *output, AbError *error) {
    (void)error;
    MuseumFixture *fixture = context;
    MuseumCaseValue receiver = encode(fixture, object);
    CHECK(receiver.tag == 5 && (receiver.value == 3 || receiver.value == 4));
    bool specialty = name.size == 9 && !memcmp(name.data, "specialty", 9);
    CHECK(specialty || (name.size == 14 && !memcmp(name.data, "launchVelocity", 14)));
    MuseumCaseValue result = specialty
                                 ? museum_specialty(fixture->input, receiver.value)
                                 : museum_launch(fixture->input, receiver.value);
    CHECK(fixture->count < 4);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = 5;
    event[1] = receiver.tag;
    event[2] = receiver.value;
    event[3] = result.tag;
    event[4] = result.value;
    event[6] = specialty ? 1u : 2u;
    event[7] = fixture->count;
    *output = value(fixture, result);
    return mutate(fixture);
}

static void fields(const MuseumFixture *fixture, uint32_t output[8]) {
    const char *const names[] = {"name", "gameObject", "physicsObject", "flying"};
    for (size_t index = 0; index < 4; ++index) {
        const AbDataValue *field =
            ab_data_get(&fixture->behavior.fields, text(names[index]));
        MuseumCaseValue word = encode(fixture, field ? *field : (AbDataValue){0});
        output[index * 2] = word.tag;
        output[index * 2 + 1] = word.value;
    }
}

static bool prepare(MuseumFixture *fixture, uint32_t sample) {
    museum_behavior_case(sample, fixture->input);
    const uint32_t *input = fixture->input;
    for (uint32_t index = 0; index < 2; ++index) {
        fixture->physical[index][0] = (AbDataEntry){
            text("definition"), value(fixture, museum_definition(input, index + 1u))};
        fixture->tables[index] = (AbDataTable){fixture->physical[index], 1, 1, NULL};
        fixture->scripts[index][0] =
            (AbDataEntry){text("physicsObject"),
                          value(fixture, index ? (MuseumCaseValue){5, 2}
                                               : museum_scalar(input[3], input[8], 1))};
        fixture->tables[index + 2] = (AbDataTable){fixture->scripts[index], 1, 1, NULL};
        fixture->launch[index][0] = (AbDataEntry){
            text("x"), value(fixture, museum_launch_x(input, index + 6u))};
        fixture->tables[index + 5] = (AbDataTable){fixture->launch[index], 1, 1, NULL};
    }
    fixture->other[0] = (AbDataEntry){
        text("controllable"), value(fixture, museum_scalar(input[4], input[8], 8))};
    fixture->other[1] = (AbDataEntry){
        text("name"), value(fixture, museum_scalar(input[5], input[8], 8))};
    fixture->tables[4] = (AbDataTable){fixture->other, 2, 2, NULL};
    AbDataValue overrides =
        sample % 3u == 0 ? (AbDataValue){0}
        : sample % 3u == 1
            ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
            : (AbDataValue){.kind = AB_DATA_TABLE, .table = &fixture->empty};
    const char *class_name = strings[input[1] ? 5 : 4];
    CHECK(ab_museum_bird_behavior_initialize(text(class_name).string,
                                             value(fixture, (MuseumCaseValue){5, 3}),
                                             overrides, &fixture->behavior, NULL));
    fields(fixture, fixture->initial);
    CHECK(set(fixture, "flying", value(fixture, museum_scalar(input[7], input[8], 8))));
    const MuseumCaseValue flying[] = {{0, 0}, museum_scalar(input[3], input[8], 1),
                                      {5, 2}, {1, 0},
                                      {1, 1}, {3, input[8]}};
    fixture->host =
        (AbMuseumBirdHost){fixture, value(fixture, flying[input[6]]), service,
                           input[15] & 16u ? property : NULL, NULL};
    return true;
}

static bool compare(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                    const char *phase) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Museum behavior %u %s word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, phase, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool constructor_limits(void) {
    AbMuseumBirdBehavior output;
    memset(&output, 0xa5, sizeof(output));
    uint8_t saved[sizeof(output)];
    memcpy(saved, &output, sizeof(saved));
    AbDataTable empty = {0};
    AbDataValue receiver = {.kind = AB_DATA_TABLE, .table = &empty};
    CHECK(!ab_museum_bird_behavior_initialize(text("Unknown").string, receiver,
                                              (AbDataValue){0}, &output, NULL) &&
          !memcmp(&output, saved, sizeof(output)));
    AbSpan name = text(strings[4]).string;
    CHECK(!ab_museum_bird_behavior_initialize(name, (AbDataValue){0}, (AbDataValue){0},
                                              &output, NULL) &&
          !memcmp(&output, saved, sizeof(output)));
    AbDataEntry entry = {text("parameter"), {.kind = AB_DATA_BOOLEAN}};
    AbDataTable overrides = {&entry, 1, 1, NULL};
    CHECK(!ab_museum_bird_behavior_initialize(
              name, receiver, (AbDataValue){.kind = AB_DATA_TABLE, .table = &overrides},
              &output, NULL) &&
          !memcmp(&output, saved, sizeof(output)));
    CHECK(ab_museum_bird_behavior_initialize(name, receiver, (AbDataValue){0}, &output,
                                             NULL) &&
          output.fields.entries == output.entries && output.fields.count == 2);
    entry = (AbDataEntry){text("physicsObject"), {.kind = AB_DATA_BOOLEAN}};
    AbDataTable script = {&entry, 1, 1, NULL};
    receiver.table = &script;
    CHECK(ab_museum_bird_behavior_initialize(name, receiver, (AbDataValue){0}, &output,
                                             NULL) &&
          output.fields.count == 3 && output.entries[2].value.kind == AB_DATA_BOOLEAN &&
          !output.entries[2].value.bits);
    return true;
}

static bool reference(FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_MUSEUM_BIRD_BEHAVIOR_V1 2048\n"));
    size_t failures = 0, events = 0;
    for (uint32_t sample = 0; sample < MUSEUM_BEHAVIOR_CASES; ++sample) {
        MuseumFixture fixture = {0};
        CHECK(prepare(&fixture, sample));
        AbMuseumBirdBehavior *behavior = &fixture.behavior;
        AbMuseumBirdHost *host = &fixture.host;
        bool okay =
            fixture.input[2] == 0 ? ab_museum_bird_behavior_launch(behavior, host, NULL)
            : fixture.input[2] == 1
                ? ab_museum_bird_behavior_collision(
                      behavior, value(&fixture, (MuseumCaseValue){5, 5}), host, NULL)
            : fixture.input[2] == 2
                ? ab_museum_bird_behavior_destroy(behavior, host, NULL)
                : ab_museum_bird_behavior_specialty(behavior, host, NULL);
        uint32_t output[20] = {!okay, fixture.count};
        memcpy(output + 2, fixture.initial, sizeof(fixture.initial));
        fields(&fixture, output + 10);
        output[18] = fixture.mutated;
        CHECK(compare(file, fixture.input, 16, sample, "input") &&
              compare(file, output, 20, sample, "state"));
        for (uint32_t index = 0; index < fixture.count; ++index)
            CHECK(compare(file, fixture.events[index], 8, sample, "event"));
        failures += !okay;
        events += fixture.count;
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB museum bird behavior reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %u actual Museum constructors/events, %zu services, %zu failures\n",
           MUSEUM_BEHAVIOR_CASES, events, failures);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 1 && argc != 2)
        return 2;
    if (!constructor_limits())
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
