#include "ab/bird_collision_prelude.h"
#include "bird_prelude_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird prelude failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionPreludeHost callbacks;
    AbBirdCollisionPreludeState state;
    AbBirdCollisionObject objects[3];
    AbBirdCollisionHat hats[2];
    AbBirdCollisionScoreEntry entry;
    float scores[1];
    uint32_t input[21], events[8][24], count;
    bool fail_floating;
} Fixture;

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool named(AbSpan name, const char *text) {
    return name.size == strlen(text) && name.data &&
           memcmp(name.data, text, name.size) == 0;
}

static uint32_t bits(float number) {
    uint32_t value;
    memcpy(&value, &number, sizeof(value));
    return value;
}

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue scalar(uint32_t kind) {
    static AbDataTable empty = {0};
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN};
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return (AbDataValue){.kind = AB_DATA_STRING, .string = span("truthy")};
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static void value_words(AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER
                    ? value.bits
                : value.kind == AB_DATA_STRING ? 1u
                                               : 0u;
}

static void state_words(const Fixture *fixture, uint32_t output[24]) {
    output[12] = fixture->entry.present;
    output[13] = fixture->entry.present ? bits(fixture->entry.score) : 0;
    output[14] = fixture->objects[2].defence.bits;
    output[15] = fixture->objects[2].strength.bits;
    value_words(fixture->state.delta_based_slingshot, output + 16);
    value_words(fixture->state.bird_specialty_available, output + 18);
    for (size_t id = 0; id < 2; ++id) {
        output[20 + id * 2] = truthy(fixture->objects[id].shot);
        output[21 + id * 2] = truthy(fixture->objects[id].has_collided);
    }
}

static uint32_t *record(Fixture *fixture, uint32_t kind) {
    if (fixture->count >= 8) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = kind;
    state_words(fixture, event);
    return event;
}

static bool hat(void *context, AbSpan name, AbBirdCollisionHat **output,
                AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t mode = fixture->input[1];
    *output = NULL;
    if (named(name, "actor_first")) {
        if (mode == 1 || mode == 3 || mode == 4 || mode == 6) {
            *output = fixture->hats;
        }
    } else {
        CHECK(named(name, "second"));
        if (mode == 2 || mode == 5 || mode == 6) {
            *output = fixture->hats + 1;
        }
    }
    return true;
}

static bool object(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    (void)error;
    Fixture *fixture = context;
    if (named(name, "hat")) {
        *output = fixture->objects + 2;
    } else {
        CHECK(named(name, "missing_hat"));
        *output = NULL;
    }
    return true;
}

static bool score_entry(void *context, AbSpan name, AbBirdCollisionScoreEntry **output,
                        AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(named(name, "first"));
    *output = &fixture->entry;
    return true;
}

static bool score(void *context, float damage, float *output, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 1);
    CHECK(event);
    event[1] = bits(damage);
    if (fixture->input[9] & 1u) {
        fixture->state.floating_score_scaling = 2.0f;
    }
    if (fixture->input[9] & 2u) {
        fixture->objects[0].position = (AbVec2){-0.0f, 8.0f};
    }
    *output = number(fixture->input[11]);
    return true;
}

static bool floating(void *context, const AbBirdCollisionFloatingScore *request,
                     AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 2);
    CHECK(event);
    float numbers[] = {request->position.x, request->position.y, request->score,
                       request->time,       request->lifetime,   request->max_scale,
                       request->xs};
    for (size_t field = 0; field < 7; ++field) {
        event[1 + field] = bits(numbers[field]);
    }
    event[8] = (uint32_t)request->text.size;
    event[9] = UINT32_C(2166136261);
    for (size_t index = 0; index < request->text.size; ++index) {
        event[9] = (event[9] ^ request->text.data[index]) * UINT32_C(16777619);
    }
    if (fixture->input[9] & 4u) {
        fixture->entry = (AbBirdCollisionScoreEntry){true, 100.0f};
    }
    if (fixture->input[9] & 8u) {
        fixture->state.delta_based_slingshot = scalar(1);
    }
    if (fixture->input[9] & 16u) {
        fixture->objects[0].has_collided = scalar(1);
        fixture->objects[1].has_collided = scalar(1);
    }
    if (fixture->input[9] & 32u) {
        fixture->state.bird_specialty_available = scalar(1);
    }
    if (fixture->fail_floating) {
        return ab_fail(error, "Authored floating score failure");
    }
    return true;
}

static bool cooldown(void *context, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(record(fixture, 3));
    if ((fixture->input[9] & 64u) && fixture->entry.present) {
        fixture->entry.score = -5.0f;
    }
    if (fixture->input[9] & 128u) {
        fixture->objects[0].shot = scalar(0);
    }
    return true;
}

static bool achievement(void *context, AbBirdCollisionObject *first,
                        AbBirdCollisionObject *second, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(first == fixture->objects && second == fixture->objects + 1);
    uint32_t *event = record(fixture, 4);
    CHECK(event);
    event[1] = 1;
    event[2] = 2;
    return true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_prelude_case(sample, fixture->input);
    for (size_t id = 0; id < 2; ++id) {
        fixture->objects[id].object.name = span(id ? "second" : "actor_first");
        fixture->objects[id].shot = scalar(fixture->input[2 + id]);
        fixture->objects[id].has_collided = scalar(fixture->input[4 + id]);
        fixture->objects[id].position = (AbVec2){number(fixture->input[id ? 18 : 14]),
                                                 number(fixture->input[id ? 19 : 15])};
    }
    fixture->objects[2].defence =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[16]};
    fixture->objects[2].strength =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[17]};
    fixture->hats[0].object_name = span(fixture->input[1] == 1   ? "second"
                                        : fixture->input[1] == 4 ? "missing_hat"
                                                                 : "hat");
    fixture->hats[1].object_name = span(fixture->input[1] == 2 ? "actor_first" : "hat");
    fixture->entry =
        (AbBirdCollisionScoreEntry){fixture->input[8] != 0, number(fixture->input[13])};
    fixture->state = (AbBirdCollisionPreludeState){
        .delta_based_slingshot = scalar(fixture->input[6]),
        .bird_specialty_available = scalar(fixture->input[7]),
        .floating_score_scaling = number(fixture->input[12]),
        .collision_scores = fixture->scores,
        .capacity = 1};
    fixture->callbacks = (AbBirdCollisionPreludeHost){
        .context = fixture,
        .hat = hat,
        .object = object,
        .score_entry = score_entry,
        .score = score,
        .floating = floating,
        .cooldown = cooldown,
        .achievement = fixture->input[20] ? achievement : NULL};
}

static bool invoke(Fixture *fixture, bool *related, AbError *error) {
    return ab_bird_collision_prelude(span("first"), fixture->objects,
                                     fixture->objects + 1, number(fixture->input[10]),
                                     &fixture->state, &fixture->callbacks, related,
                                     error);
}

static bool edges(void) {
    Fixture fixture;
    AbError error = {0};
    bool related = true;
    prepare(&fixture, 1);
    fixture.input[1] = 1;
    fixture.hats[0].object_name = span("second");
    fixture.input[10] = bits(NAN);
    fixture.callbacks.score = NULL;
    CHECK(invoke(&fixture, &related, &error) && related && !fixture.count);

    prepare(&fixture, 1);
    fixture.input[1] = 3;
    fixture.hats[0].object_name = span("hat");
    fixture.objects[0].shot = scalar(2);
    fixture.state.capacity = 0;
    CHECK(!invoke(&fixture, &related, &error) && related && fixture.count == 2 &&
          fixture.entry.present && fixture.state.count == 0 &&
          fixture.objects[2].defence.bits == 0 &&
          fixture.objects[2].strength.bits == 0);

    prepare(&fixture, 1);
    fixture.fail_floating = true;
    float old = fixture.entry.score;
    CHECK(!invoke(&fixture, &related, &error) && fixture.entry.score == old &&
          fixture.state.count == 0 && fixture.count == 2);

    prepare(&fixture, 1);
    fixture.callbacks.cooldown = NULL;
    fixture.objects[0].shot = scalar(2);
    CHECK(!invoke(&fixture, &related, &error) && fixture.entry.present &&
          fixture.state.count == 1 && fixture.count == 2);

    prepare(&fixture, 1);
    fixture.input[1] = 4;
    fixture.hats[0].object_name = span("missing_hat");
    fixture.objects[0].shot = scalar(2);
    uint32_t defence = fixture.objects[2].defence.bits;
    CHECK(invoke(&fixture, &related, &error) && !related && fixture.state.count == 1 &&
          fixture.objects[2].defence.bits == defence);
    puts("Bird prelude guard, mutation and failure-prefix controls pass.");
    return true;
}

static bool compare_words(FILE *file, const uint32_t *actual, size_t count,
                          uint32_t sample, const char *phase, size_t *words) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Bird prelude case %u %s word %zu: %08" PRIx32
                    " expected %08" PRIx32 "\n",
                    sample, phase, field, actual[field], expected);
            return false;
        }
        ++*words;
    }
    return true;
}

static bool reference(const char *path) {
    FILE *file = fopen(path, "r");
    CHECK(file);
    char header[96];
    bool okay = fgets(header, sizeof(header), file) &&
                strcmp(header, "AB_ANDROID_BIRD_PRELUDE_V1 4096\n") == 0;
    size_t words = 0, counts[6] = {0}, skips = 0, failures = 0, appends = 0;
    for (uint32_t sample = 0; okay && sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        okay = compare_words(file, fixture.input, 21, sample, "input", &words);
        AbError error = {0};
        bool related = false;
        bool success = invoke(&fixture, &related, &error);
        failures += !success;
        skips += success && related;
        if (success && !related) {
            okay = okay && record(&fixture, 5) != NULL;
        }
        uint32_t result[24] = {!success, success && related};
        state_words(&fixture, result);
        okay = okay && compare_words(file, result, 24, sample, "result", &words);
        uint32_t count = (uint32_t)fixture.state.count;
        appends += count;
        okay = okay && compare_words(file, &count, 1, sample, "appends", &words);
        if (count) {
            uint32_t value = bits(fixture.scores[0]);
            okay = okay && compare_words(file, &value, 1, sample, "append", &words);
        }
        okay = okay && compare_words(file, &fixture.count, 1, sample, "events", &words);
        for (uint32_t event = 0; okay && event < fixture.count; ++event) {
            ++counts[fixture.events[event][0]];
            okay =
                compare_words(file, fixture.events[event], 24, sample, "event", &words);
        }
    }
    if (okay) {
        char first[16], second[16], third[16], fourth[16], fifth[2];
        okay = fscanf(file, "%15s %15s %15s %15s", first, second, third, fourth) == 4 &&
               !strcmp(first, "AB") && !strcmp(second, "bird") &&
               !strcmp(third, "prelude") && !strcmp(fourth, "reference") &&
               fscanf(file, "%15s", header) == 1 && !strcmp(header, "complete") &&
               fscanf(file, "%1s", fifth) == EOF;
    }
    fclose(file);
    if (okay) {
        printf("All 4,096 original bird prelude cases match: %zu words, %zu related "
               "hat skips, %zu failures, %zu score appends.\n",
               words, skips, failures, appends);
        for (size_t kind = 1; kind < 6; ++kind) {
            printf("Prelude boundary %zu: %zu calls\n", kind, counts[kind]);
        }
    }
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc == 2) {
        return reference(argv[1]) ? 0 : 1;
    }
    return 2;
}
