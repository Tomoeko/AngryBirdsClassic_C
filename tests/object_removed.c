#include "ab/object_removed.h"
#include "object_removed_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Object-removed check failed at %d: %s\n", __LINE__,       \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[OBJECT_REMOVED_INPUT_WORDS], check_binding;
    AbObjectRemovedState state;
    AbObjectRemovedHost host;
    AbDataTable tables[10];
    AbDataEntry entries[10][8];
    AbStringMap *goals[2];
    uint32_t events[2][OBJECT_REMOVED_EVENT_WORDS];
    size_t event_count;
} RemovedFixture;

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(uint32_t bits) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool set(AbDataTable *target, const char *name, AbDataValue value) {
    AbDataValue key = text(name);
    for (size_t index = 0; index < target->count; ++index) {
        AbDataEntry *entry = target->entries + index;
        if (entry->key.kind == AB_DATA_STRING &&
            entry->key.string.size == key.string.size &&
            !memcmp(entry->key.string.data, key.string.data, key.string.size)) {
            entry->value = value;
            return true;
        }
    }
    CHECK(target->count < target->capacity);
    target->entries[target->count++] = (AbDataEntry){key, value};
    return true;
}

static AbDataValue get(const AbDataTable *source, const char *name) {
    const AbDataValue *value = ab_data_get(source, text(name));
    return value ? *value : (AbDataValue){0};
}

static AbDataValue scalar(RemovedFixture *fixture, uint32_t kind) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    if (kind == 3) {
        return number(0);
    }
    return kind == 4 ? text("truthy") : table(fixture->tables + 9);
}

static bool encode(AbDataValue value, uint32_t output[2]) {
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    if (value.kind == AB_DATA_NUMBER) {
        output[0] = 3;
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits != 0;
    } else if (value.kind == AB_DATA_STRING) {
        output[0] = 4;
        CHECK(value.string.size == 6 && !memcmp(value.string.data, "truthy", 6));
        output[1] = 1;
    } else if (value.kind == AB_DATA_TABLE) {
        output[0] = 5;
        AbDataValue id = get(value.table, "_id");
        CHECK(id.kind == AB_DATA_NUMBER && id.bits >= 201 && id.bits <= 210);
        output[1] = id.bits;
    } else {
        CHECK(value.kind == AB_DATA_NIL);
    }
    return true;
}

static bool snapshot(RemovedFixture *fixture,
                     uint32_t output[OBJECT_REMOVED_STATE_WORDS]) {
    AbDataValue goal = {.kind = AB_DATA_BOOLEAN};
    if (fixture->state.level_goals) {
        CHECK(fixture->state.level_goals == fixture->goals[0] ||
              fixture->state.level_goals == fixture->goals[1]);
        goal = table(fixture->tables +
                     (fixture->state.level_goals == fixture->goals[1] ? 7 : 6));
    }
    AbDataValue globals[] = {fixture->state.objects,
                             goal,
                             fixture->state.last_score_time,
                             fixture->state.old_score,
                             fixture->state.time,
                             fixture->state.score,
                             fixture->state.seconds};
    size_t next = 0;
    for (size_t index = 0; index < 7; ++index) {
        CHECK(encode(globals[index], output + next));
        next += 2;
    }
    for (size_t index = 2; index < 8; ++index) {
        AbDataValue value = get(fixture->tables + index, "Object");
        if (index >= 6) {
            bool present = false;
            uintptr_t handle = 0;
            CHECK(ab_string_map_get(fixture->goals[index - 6], span("Object"), &handle,
                                    &present, NULL));
            value = present ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}
                            : (AbDataValue){0};
        }
        CHECK(encode(value, output + next));
        next += 2;
    }
    for (size_t index = 0; index < 2; ++index) {
        CHECK(encode(get(fixture->tables + index, "world"), output + next));
        CHECK(encode(get(fixture->tables + index, "dynamic"), output + next + 2));
        next += 4;
    }
    CHECK(next == OBJECT_REMOVED_STATE_WORDS);
    return true;
}

static bool mutate(RemovedFixture *fixture, uint32_t kind) {
    uint32_t flags = fixture->input[9];
    if (flags & 1u) {
        fixture->state.objects = table(fixture->tables + 1);
    }
    for (uint32_t field = 0; field < 2; ++field) {
        if (flags & (2u << field)) {
            CHECK(set(fixture->state.objects.table, field ? "dynamic" : "world",
                      (AbDataValue){.kind = AB_DATA_BOOLEAN}));
        }
    }
    if (flags & 8u) {
        fixture->state.level_goals = fixture->goals[1];
    }
    if (flags & 16u) {
        fixture->state.level_goals = NULL;
    }
    if (flags & 32u) {
        fixture->check_binding = 0;
    }
    if (flags & 64u) {
        fixture->state.seconds = number(fixture->input[10] ^ kind);
        fixture->state.time = number(fixture->input[11] ^ kind);
        fixture->state.score = number(fixture->input[12] ^ kind);
    }
    if (flags & 128u) {
        CHECK(set(fixture->tables + 8, "controllable",
                  (AbDataValue){.kind = AB_DATA_BOOLEAN}));
    }
    return true;
}

static bool event(RemovedFixture *fixture, uint32_t kind, uint32_t pc,
                  AbDataValue object, AbDataValue seconds) {
    CHECK(fixture->event_count < 2);
    uint32_t *output = fixture->events[fixture->event_count++];
    output[0] = pc;
    output[1] = kind;
    output[2] = kind == 3 ? 0u : kind == 2 ? 2u : 1u;
    output[3] = output[4] = output[5] = 0;
    if (kind != 3) {
        CHECK(object.kind == AB_DATA_TABLE && object.table == fixture->tables + 8);
        output[3] = 209;
    }
    if (kind == 2) {
        CHECK(encode(seconds, output + 4));
    }
    return snapshot(fixture, output + 6) && mutate(fixture, kind);
}

static bool exited(void *context, AbDataValue object, AbDataValue seconds,
                   AbError *error) {
    RemovedFixture *fixture = context;
    if (fixture->input[5] != 5) {
        return ab_fail(error, "Controlled non-callable exitedFunction");
    }
    return event(fixture, 2, 17, object, seconds);
}

static bool notify(void *context, AbDataValue object, AbError *error) {
    (void)error;
    return event(context, 1, 21, object, (AbDataValue){0});
}

static bool check_goals(void *context, AbDataValue *result, AbError *error) {
    RemovedFixture *fixture = context;
    if (fixture->check_binding != 5) {
        return ab_fail(error, "Controlled non-callable goal check");
    }
    CHECK(event(fixture, 3, 31, (AbDataValue){0}, (AbDataValue){0}));
    *result = scalar(fixture, fixture->input[8]);
    return true;
}

static bool prepare(RemovedFixture *fixture) {
    for (size_t index = 0; index < 10; ++index) {
        fixture->tables[index] = (AbDataTable){fixture->entries[index], 0, 8, NULL};
        /* The fixture stores diagnostic IDs as raw words, without arithmetic. */
        CHECK(set(fixture->tables + index, "_id", number((uint32_t)index + 201u)));
    }
    for (size_t index = 2; index < 8; ++index) {
        AbDataValue value = {.kind = AB_DATA_BOOLEAN, .bits = 1};
        if (index < 4) {
            value = fixture->input[1] == 5 ? table(fixture->tables + 8)
                                           : scalar(fixture, fixture->input[1]);
        }
        CHECK(set(fixture->tables + index, "Object", value));
    }
    for (size_t index = 0; index < 2; ++index) {
        CHECK(
            set(fixture->tables + index, "world", table(fixture->tables + 2 + index)));
        CHECK(set(fixture->tables + index, "dynamic",
                  table(fixture->tables + 4 + index)));
        CHECK(ab_string_map_open(0, 8, 8192, fixture->goals + index, NULL));
        CHECK(ab_string_map_set(fixture->goals[index], span("Object"), 1, true, NULL));
    }
    const char *const fields[] = {"controllable", "isMightyBait", "isMightyEagle"};
    for (size_t index = 0; index < 3; ++index) {
        CHECK(set(fixture->tables + 8, fields[index],
                  scalar(fixture, fixture->input[2 + index])));
    }
    fixture->state =
        (AbObjectRemovedState){.objects = table(fixture->tables),
                               .level_goals = fixture->goals[0],
                               .seconds = number(fixture->input[10]),
                               .time = number(fixture->input[11]),
                               .score = number(fixture->input[12]),
                               .last_score_time = number(UINT32_C(0x41400000)),
                               .old_score = number(UINT32_C(0x41a00000))};
    fixture->check_binding = fixture->input[7];
    fixture->host = (AbObjectRemovedHost){
        fixture, exited, fixture->input[6] == 2 ? notify : NULL, check_goals};
    return true;
}

static void close_fixture(RemovedFixture *fixture) {
    ab_string_map_close(fixture->goals[0]);
    ab_string_map_close(fixture->goals[1]);
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected = 0;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[index]) {
            fprintf(stderr,
                    "Object-removed case %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool compare_case(FILE *file, RemovedFixture *fixture, uint32_t sample,
                         size_t *matched, size_t *calls, size_t *failures) {
    CHECK(words(file, fixture->input, OBJECT_REMOVED_INPUT_WORDS, sample));
    AbError error = {0};
    uint32_t failed =
        !ab_object_removed(&fixture->state, span("Object"), &fixture->host, &error);
    CHECK(words(file, &failed, 1, sample));
    uint32_t state[OBJECT_REMOVED_STATE_WORDS];
    CHECK(snapshot(fixture, state) &&
          words(file, state, OBJECT_REMOVED_STATE_WORDS, sample));
    uint32_t count = (uint32_t)fixture->event_count;
    CHECK(words(file, &count, 1, sample));
    for (size_t index = 0; index < fixture->event_count; ++index) {
        CHECK(words(file, fixture->events[index], OBJECT_REMOVED_EVENT_WORDS, sample));
    }
    *matched += OBJECT_REMOVED_INPUT_WORDS + 2 + OBJECT_REMOVED_STATE_WORDS +
                OBJECT_REMOVED_EVENT_WORDS * fixture->event_count;
    *calls += fixture->event_count;
    *failures += failed;
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_OBJECT_REMOVED_V1 4096\n"));
    size_t matched = 0, calls = 0, failures = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        RemovedFixture fixture = {0};
        object_removed_case(sample, fixture.input);
        bool okay = prepare(&fixture) &&
                    compare_case(file, &fixture, sample, &matched, &calls, &failures);
        close_fixture(&fixture);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB object removed reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original object-removed cases match: %zu words, %zu checked "
           "callback boundaries, %zu partial failures.\n",
           matched, calls, failures);
    return true;
}

static bool controls(void) {
    RemovedFixture fixture = {0};
    object_removed_case(0, fixture.input);
    bool okay = prepare(&fixture);
    if (okay) {
        AbError error = {0};
        okay =
            ab_object_removed(&fixture.state, span("Object"), &fixture.host, &error) &&
            fixture.event_count == 2 &&
            get(fixture.tables + 2, "Object").kind == AB_DATA_NIL &&
            get(fixture.tables + 4, "Object").kind == AB_DATA_NIL &&
            fixture.state.last_score_time.bits == fixture.state.time.bits &&
            fixture.state.old_score.bits == fixture.state.score.bits;
    }
    close_fixture(&fixture);
    CHECK(okay);
    fixture = (RemovedFixture){0};
    object_removed_case(0, fixture.input);
    fixture.input[2] = 2;
    fixture.input[3] = fixture.input[4] = 0;
    okay = prepare(&fixture);
    if (okay) {
        okay = ab_object_removed(&fixture.state, span("Object"), NULL, NULL) &&
               fixture.event_count == 0 &&
               get(fixture.tables + 2, "Object").kind == AB_DATA_TABLE;
    }
    close_fixture(&fixture);
    CHECK(okay);
    static const uint32_t mutations[] = {1, 2, 4, 16};
    for (size_t index = 0; index < sizeof(mutations) / sizeof(*mutations); ++index) {
        fixture = (RemovedFixture){0};
        object_removed_case(0, fixture.input);
        uint32_t mutation = mutations[index];
        fixture.input[9] = mutation;
        okay = prepare(&fixture);
        if (okay) {
            bool success =
                ab_object_removed(&fixture.state, span("Object"), &fixture.host, NULL);
            bool original_world_cleared = mutation == 4 || mutation == 16;
            okay = success == (mutation == 1) &&
                   fixture.event_count == (success ? 2u : 1u) &&
                   (get(fixture.tables + 2, "Object").kind == AB_DATA_NIL) ==
                       original_world_cleared &&
                   (get(fixture.tables + 3, "Object").kind == AB_DATA_NIL) ==
                       (mutation == 1) &&
                   (get(fixture.tables + 4, "Object").kind == AB_DATA_NIL) ==
                       (mutation == 16);
        }
        close_fixture(&fixture);
        CHECK(okay);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return controls() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
