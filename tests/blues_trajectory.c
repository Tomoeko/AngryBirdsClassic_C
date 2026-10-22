#include "ab/museum_bird_behavior.h"
#include "ab/object_property_access.h"
#include "blues_trajectory_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Blues check at %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[BLUES_INPUT_WORDS], events[4][BLUES_EVENT_WORDS], event_count;
    BluesEnvironment environment;
    AbMuseumBirdBehavior behavior;
    AbBluesTrajectoryHost host;
    AbDataValue live_flying, live_others;
    bool live_globals;
    AbDataTable tables[8];
    AbDataEntry script, other[2], lists[2][2];
} BluesFixture;

static const char *const field_names[] = {"isCluster", "flying", "split",
                                          "physicsObject", "originalCluster"};
static const char *const strings[] = {"", "RedBird", "Other", "BOOMERANG"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue value(BluesFixture *fixture, BluesValue encoded) {
    if (encoded.tag == 5)
        return (AbDataValue){.kind = AB_DATA_TABLE,
                             .table = fixture->tables + encoded.value - 1};
    if (encoded.tag == 4)
        return text(strings[encoded.value]);
    return (AbDataValue){.kind = encoded.tag == 3   ? AB_DATA_NUMBER
                                 : encoded.tag == 1 ? AB_DATA_BOOLEAN
                                                    : AB_DATA_NIL,
                         .bits = encoded.value};
}

static BluesValue encode(BluesFixture *fixture, AbDataValue input) {
    if (input.kind == AB_DATA_TABLE) {
        for (uint32_t index = 0; index < 8; ++index)
            if (input.table == fixture->tables + index)
                return (BluesValue){5, index + 1};
        return (BluesValue){5, UINT32_MAX};
    }
    if (input.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 4; ++index)
            if (input.string.size == strlen(strings[index]) &&
                !memcmp(input.string.data, strings[index], input.string.size))
                return (BluesValue){4, index};
        return (BluesValue){4, UINT32_MAX};
    }
    return (BluesValue){input.kind == AB_DATA_NUMBER    ? 3u
                        : input.kind == AB_DATA_BOOLEAN ? 1u
                                                        : 0u,
                        input.bits};
}

static bool write_state(BluesFixture *fixture) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = &fixture->behavior.fields}};
    for (size_t index = 0; index < 5; ++index)
        CHECK(ab_object_property_set(&access, text(field_names[index]).string,
                                     value(fixture, fixture->environment.fields[index]),
                                     NULL));
    if (fixture->live_globals) {
        fixture->live_flying = value(fixture, fixture->environment.flying);
        fixture->live_others = value(fixture, fixture->environment.others);
    } else {
        fixture->host.flying_bird = value(fixture, fixture->environment.flying);
        fixture->host.other_birds = value(fixture, fixture->environment.others);
    }
    return true;
}

static void read_state(BluesFixture *fixture) {
    for (size_t index = 0; index < 5; ++index) {
        const AbDataValue *found =
            ab_data_get(&fixture->behavior.fields, text(field_names[index]));
        fixture->environment.fields[index] =
            encode(fixture, found ? *found : (AbDataValue){0});
    }
    fixture->environment.flying =
        encode(fixture, fixture->live_globals ? fixture->live_flying
                                              : fixture->host.flying_bird);
    fixture->environment.others =
        encode(fixture, fixture->live_globals ? fixture->live_others
                                              : fixture->host.other_birds);
}

static bool service(void *context, AbBluesTrajectoryCall *call, AbError *error) {
    (void)error;
    BluesFixture *fixture = context;
    uint32_t operation = (uint32_t)call->operation;
    if (fixture->input[12] == operation ||
        (operation == 4 && fixture->input[13] == fixture->environment.get_calls + 1))
        return false;
    CHECK(fixture->event_count < 4);
    read_state(fixture);
    if (operation == 4)
        ++fixture->environment.get_calls;
    uint32_t *event = fixture->events[fixture->event_count++];
    event[0] = operation;
    if (operation == 3) {
        CHECK(call->keep_slots == 1 || call->keep_slots == 4);
        for (size_t index = 0; index < 4; ++index) {
            BluesValue encoded = encode(fixture, call->keep[index]);
            event[3] += encoded.tag != 0;
            event[4 + index * 2] = encoded.tag;
            event[5 + index * 2] = encoded.value;
        }
    } else {
        BluesValue encoded =
            encode(fixture, operation == 4 ? call->name : call->object);
        event[1] = encoded.tag;
        event[2] = encoded.value;
    }
    blues_snapshot(&fixture->environment, event + 12);
    blues_mutate(fixture->input, &fixture->environment);
    CHECK(write_state(fixture));
    if (operation == 4) {
        uint32_t get = fixture->environment.get_calls;
        call->result = value(fixture, fixture->input[10] & (get == 1 ? 1u : 2u)
                                          ? (BluesValue){0, 0}
                                          : blues_physical(get == 1 ? 2u : 1u));
    }
    return true;
}

static bool prepare(BluesFixture *fixture) {
    fixture->script =
        (AbDataEntry){text("physicsObject"), value(fixture, blues_physical(1))};
    fixture->tables[2] = (AbDataTable){&fixture->script, 1, 1, NULL};
    AbDataEntry parameter = {text("isCluster"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable parameters = {&parameter, 1, 1, NULL};
    CHECK(ab_museum_bird_behavior_initialize(
        text("MuseumBluesTrajectoryRenderer").string,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + 2},
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &parameters}, &fixture->behavior,
        NULL));
    CHECK(fixture->behavior.kind == AB_MUSEUM_BIRD_BLUES &&
          fixture->behavior.fields.count == 4 &&
          ab_museum_bird_behavior_init(&fixture->behavior, NULL, NULL));
    fixture->host = (AbBluesTrajectoryHost){.context = fixture, .call = service};
    if (fixture->live_globals) {
        fixture->host.live_flying_bird = &fixture->live_flying;
        fixture->host.live_other_birds = &fixture->live_others;
        /* Prepared fields intentionally differ. Actual source reads must use
         * the canonical fields, including changes made by service callbacks. */
        fixture->host.flying_bird = text("unused");
        fixture->host.other_birds = text("unused");
    }
    blues_environment(fixture->input, &fixture->environment);
    CHECK(write_state(fixture));
    const BluesValue names[] = {{0, 0}, {1, 0}, {4, 0}, {4, 2}, {4, 1}};
    fixture->other[0] = (AbDataEntry){
        text("controllable"), value(fixture, blues_predicate(fixture->input[7]))};
    fixture->other[1] =
        (AbDataEntry){text("name"), value(fixture, names[fixture->input[8]])};
    fixture->tables[4] = (AbDataTable){fixture->other, 2, 2, NULL};
    for (size_t list = 0; list < 2; ++list) {
        fixture->lists[list][0] =
            (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                          text(list ? "Other" : "RedBird")};
        fixture->lists[list][1] =
            (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)},
                          list ? (AbDataValue){0} : text("Other")};
        fixture->tables[list + 6] = (AbDataTable){fixture->lists[list], 2, 2, NULL};
    }
    return true;
}

static bool event(BluesFixture *fixture, uint32_t output[17]) {
    AbMuseumBirdHost host = {.blues = &fixture->host};
    bool okay;
    switch (fixture->input[1]) {
        case 0:
            okay = ab_museum_bird_behavior_init(&fixture->behavior, &host, NULL);
            break;
        case 1:
            okay = ab_museum_bird_behavior_launch(&fixture->behavior, &host, NULL);
            break;
        case 2:
            okay = ab_museum_bird_behavior_specialty(&fixture->behavior, &host, NULL);
            break;
        case 3:
            okay = ab_museum_bird_behavior_collision(
                &fixture->behavior,
                (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + 4},
                &host, NULL);
            break;
        default:
            okay = ab_museum_bird_behavior_destroy(&fixture->behavior, &host, NULL);
            break;
    }
    read_state(fixture);
    output[0] = okay;
    output[1] = fixture->event_count;
    blues_snapshot(&fixture->environment, output + 2);
    return true;
}

static bool compare(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                    const char *stage) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr, "Blues %u %s word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, stage, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool native_check(FILE *file, bool live_globals) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_BLUES_TRAJECTORY_V1 512\n"));
    size_t events = 0;
    for (uint32_t sample = 0; sample < BLUES_CASES; ++sample) {
        BluesFixture fixture = {.live_globals = live_globals};
        uint32_t output[17];
        blues_case(sample, fixture.input);
        CHECK(compare(file, fixture.input, BLUES_INPUT_WORDS, sample, "input") &&
              prepare(&fixture) && event(&fixture, output) &&
              compare(file, output, 17, sample, "final"));
        for (uint32_t index = 0; index < fixture.event_count; ++index)
            CHECK(compare(file, fixture.events[index], BLUES_EVENT_WORDS, sample,
                          "event"));
        events += fixture.event_count;
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB Blues trajectory reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 512 original Blues constructor/event cases and %zu ordered "
           "mutable service boundaries match with %s globals.\n",
           events, live_globals ? "live borrowed" : "prepared");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("Usage: ab_blues_trajectory_tests ORIGINAL_BLUES_TRACE\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_check(file, false) && !fseek(file, 0, SEEK_SET) &&
                native_check(file, true);
    if (file && fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
