#include "ab/bird_collision_mighty.h"
#include "bird_mighty_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Mighty collision failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject objects[3];
    AbBirdCollisionMightyInput input;
    AbBirdCollisionMightyHost host;
    uint32_t words[8], binding, count, event[17], lookups;
    bool fail_call;
} Fixture;

static const char *const names[] = {"first", "second", "third", "missing", "renamed"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool named(AbSpan name, const char *text) {
    return name.size == strlen(text) && name.data &&
           !memcmp(name.data, text, name.size);
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t id = 0; id < 5; ++id) {
        if (named(name, names[id])) {
            return id + 1u;
        }
    }
    return 0;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue scalar(uint32_t kind) {
    static AbDataTable empty;
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return (AbDataValue){.kind = AB_DATA_STRING, .string = span("truthy")};
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static size_t metadata_index(const Fixture *fixture) {
    return fixture->words[1] == 1 ? 0u : fixture->words[1] == 2 ? 1u : 2u;
}

static void value_words(AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER
                    ? value.bits
                : value.kind == AB_DATA_STRING ? 1u
                                               : 0u;
}

static void snapshot(const Fixture *fixture, uint32_t output[14]) {
    output[0] = fixture->input.flying_present ? 5u : 0u;
    output[1] = fixture->input.flying_present ? name_id(fixture->input.flying_name) : 0;
    output[2] = name_id(fixture->objects[0].object.name);
    output[3] = name_id(fixture->objects[1].object.name);
    value_words(fixture->objects[0].object.sensor, output + 4);
    value_words(fixture->objects[1].object.sensor, output + 6);
    const AbBirdCollisionObject *object = fixture->objects + metadata_index(fixture);
    value_words(object->mighty_bait, output + 8);
    value_words(object->mighty_eagle, output + 10);
    static const uint32_t types[] = {0, 1, 3, 5, 6, 6};
    output[12] = types[fixture->binding];
    output[13] = fixture->binding >= 4 ? fixture->binding - 3u : 0;
}

static bool lookup(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    (void)error;
    Fixture *fixture = context;
    ++fixture->lookups;
    uint32_t id = name_id(name);
    CHECK(id >= 1 && id <= 4);
    *output = id == 4 ? NULL : fixture->objects + id - 1u;
    return true;
}

static bool collision(void *context, AbSpan name, AbSpan other, AbError *error) {
    Fixture *fixture = context;
    CHECK(!fixture->count);
    fixture->count = 1;
    fixture->event[0] = fixture->binding - 3u;
    fixture->event[1] = name_id(name);
    fixture->event[2] = name_id(other);
    CHECK(fixture->event[1] && fixture->event[2]);
    snapshot(fixture, fixture->event + 3);
    uint32_t mutation = fixture->words[7];
    if (mutation & 1u) {
        fixture->objects[0].object.sensor = scalar(1);
    }
    if (mutation & 2u) {
        fixture->objects[1].object.sensor = scalar(2);
    }
    if (mutation & 4u) {
        fixture->input.flying_present = false;
    }
    AbBirdCollisionObject *object = fixture->objects + metadata_index(fixture);
    if (mutation & 8u) {
        object->object.name = span("renamed");
        fixture->input.flying_name = span("renamed");
    }
    if (mutation & 16u) {
        object->collision_function.call = NULL;
        fixture->binding = 0;
    }
    if (mutation & 32u) {
        object->mighty_bait = scalar(1);
    }
    if (fixture->fail_call) {
        return ab_fail(error, "Authored mighty collision failure");
    }
    return true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_mighty_case(sample, fixture->words);
    for (size_t index = 0; index < 3; ++index) {
        fixture->objects[index].object.name = span(names[index]);
    }
    fixture->objects[0].object.sensor = scalar(fixture->words[4]);
    fixture->objects[1].object.sensor = scalar(fixture->words[5]);
    AbBirdCollisionObject *object = fixture->objects + metadata_index(fixture);
    object->mighty_bait = scalar(fixture->words[2]);
    object->mighty_eagle = scalar(fixture->words[3]);
    fixture->binding = fixture->words[6];
    object->collision_function =
        (AbBirdCollisionFunction){fixture, fixture->binding >= 4 ? collision : NULL};
    fixture->input = (AbBirdCollisionMightyInput){
        .first = span("first"),
        .second = span("second"),
        .flying_name = span(names[fixture->words[1] ? fixture->words[1] - 1u : 2u]),
        .first_object = fixture->objects,
        .second_object = fixture->objects + 1,
        .flying_present = fixture->words[1] != 0};
    fixture->host = (AbBirdCollisionMightyHost){fixture, lookup};
}

static bool invoke(Fixture *fixture, AbError *error) {
    return ab_bird_collision_mighty(&fixture->input, &fixture->host, error);
}

static uint32_t cutoff(const Fixture *fixture, bool success) {
    if (!success) {
        return 0;
    }
    bool matches = named(fixture->input.flying_name, "first") ||
                   named(fixture->input.flying_name, "second");
    return fixture->input.flying_present && matches &&
                   !truthy(fixture->objects[1].object.sensor)
               ? 1u
               : 2u;
}

static bool edges(void) {
    Fixture fixture;
    AbError error = {0};
    prepare(&fixture, 1);
    fixture.input.flying_present = false;
    fixture.host.object = NULL;
    CHECK(invoke(&fixture, &error) && !fixture.lookups && !fixture.count);
    prepare(&fixture, 1);
    fixture.objects[0].mighty_bait = scalar(1);
    fixture.objects[0].mighty_eagle = scalar(0);
    fixture.objects[0].collision_function.call = NULL;
    CHECK(invoke(&fixture, &error) && fixture.lookups == 1 && !fixture.count);
    prepare(&fixture, 1);
    fixture.objects[0].mighty_bait = scalar(2);
    fixture.objects[1].object.sensor = scalar(1);
    fixture.fail_call = true;
    fixture.words[7] = 2;
    CHECK(!invoke(&fixture, &error) && fixture.count == 1 &&
          truthy(fixture.objects[1].object.sensor));
    prepare(&fixture, 1);
    fixture.objects[0].mighty_bait = scalar(2);
    fixture.objects[1].object.sensor = scalar(2);
    fixture.objects[0].collision_function.call = NULL;
    CHECK(invoke(&fixture, &error) && !fixture.count);
    puts(
        "Mighty collision inactive, sensor, binding and failure-prefix controls pass.");
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *phase, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Mighty collision case %u %s word %zu: %08" PRIx32
                    " expected %08" PRIx32 "\n",
                    sample, phase, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_MIGHTY_V1 4096\n"));
    size_t matched = 0, calls = 0, failures = 0, cutoffs[3] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.words, 8, sample, "input", &matched));
        AbError error = {0};
        bool success = invoke(&fixture, &error);
        uint32_t result[] = {!success, cutoff(&fixture, success)};
        ++cutoffs[result[1]];
        CHECK(words(file, result, 2, sample, "result", &matched));
        uint32_t state[14];
        snapshot(&fixture, state);
        CHECK(words(file, state, 14, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        if (fixture.count) {
            CHECK(words(file, fixture.event, 17, sample, "call", &matched));
        }
        calls += fixture.count;
        failures += !success;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird mighty reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original mighty-bird dispatch cases match: %zu words, %zu "
           "collision functions, %zu phase failures, %zu definition and %zu length "
           "suffix cutoffs.\n",
           matched, calls, failures, cutoffs[1], cutoffs[2]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
